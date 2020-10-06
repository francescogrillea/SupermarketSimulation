#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <config.h>
#include <datastruct.h>
#include <check_errors.h>

//struttura condivisa
cassiere* casse;              //array di casse lungo K
cliente* pending_client;      //lista di clienti che non hanno comprato niente e devono informare il direttore
pthread_cond_t pending_wait = PTHREAD_COND_INITIALIZER;    //gestisce la sincronizzazione tra clienti e direttore
pthread_mutex_t pending_access = PTHREAD_MUTEX_INITIALIZER; //il direttore gestisce un cliente alla volta

pthread_mutex_t write_mtx = PTHREAD_MUTEX_INITIALIZER; //scrivo nel file di log in mutua esclusione

volatile sig_atomic_t quitval = 0;   //flag binario che indica se il supermercato ha ricevuto SIGQUIT
volatile sig_atomic_t hupval = 0;   //flag binario che indica se il supermercato ha ricevuto SIGHUP

int n_open = 0;      //numero di casse aperte. Non e' necessario di accedere in mutua esclusione poiche' l'accesso a questa variabile e' sequenziale e non parallelo

int* send;                                     //array che tiene il conto del numero di clienti in coda ad una cassa (per comunicarlo al direttore)
pthread_cond_t* timer_wait;   //array di variabili di condizione necessario per poter iniare informazioni soltant da casse aperte
pthread_mutex_t sendmtx = PTHREAD_MUTEX_INITIALIZER;  //accedo all'array send in modo sicuro
static int is_send = 0;
pthread_cond_t sendwait = PTHREAD_COND_INITIALIZER;

static int n_clienti;     //numero di clienti all'interno del supermercato
pthread_mutex_t n_clientmtx = PTHREAD_MUTEX_INITIALIZER;    //accedo alla variabile n_clienti in mutua esclusione
pthread_cond_t exitclient_wait = PTHREAD_COND_INITIALIZER;  //aspetta che tutti i clienti siano usciti

//variabili globali
configurazione config;  //parametri di configurazione definiti nel file config.txt
size_t counter;  //contatore che indicizza i clienti addizionali e tiene traccia del numero di clienti totali entrati nel supermercato
FILE* fPtr;   //puntatore a file per scrivere all'interno del file di log

//===FUNZIONI===
int getClient_n();    //restituisce il numero di clienti all'interno del supermercato
void* fun_cliente(void* args);            //funzione realativa al thread cliente
void* fun_cassiere(void* args);           //funzione realtiva al thread cassiere
void* fun_direttore(void* args);          //funzione realtiva al thread direttore
void* timer(void* args);    //permette di inviare informazioni dai cassieri al direttore ogni intervallo di tempo
void* comunicazione_cliente(void* args);  //gestisce la comunicazione tra cliente e direttore (nel caso in cui il primo non abbia acquistato niente)
void* comunicazione_casse(void* args);    //gestisce la comunicazione tra cassiere e direttore (nel caso in cui il secondo voglia chiudere/aprire una cassa)
void quithandler(); //handler del segnale SIGQUIT
void huphandler();  //handler del segnale SIGHUP
void free_memory();

int main(int argc, char const *argv[]) {

  if(argc != 2){
    perror("only one argument");
    return -1;
  }
  config = read_config(argv[1]);    //legge il file config.txt
  if(!config.return_value){
    fprintf(stderr, "error while reading config file");
    return -1;
  }
  n_clienti = config.C;
  counter = config.C - 1;

  struct sigaction s_quit;    //dichiaro una sigaction in modo da gestire SIGQUIT
  if(memset(&s_quit,0,sizeof(s_quit)) == NULL){
    fprintf(stderr, "error while handling SIGQUIT");
    return -1;
  }
  s_quit.sa_handler = quithandler;
  CHECK_ZERO(sigaction(SIGQUIT,&s_quit,NULL), "error while handling SIGQUIT in sigaction");

  struct sigaction s_hup;     //dichiaro una sigaction in modo da gestire SIGHUP
  if(memset(&s_hup,0,sizeof(s_hup)) == NULL){
    fprintf(stderr, "error while handling SIGHUP");
    return -1;
  }
  s_hup.sa_handler = huphandler;
  CHECK_ZERO(sigaction(SIGHUP,&s_hup,NULL), "error while handling SIGHUP in sigaction");

  srand(time(NULL));
  if(!crea_casse()){
    fprintf(stderr, "error while creating cashier");
    return -1;
  }
  pending_client = NULL;    //inizializzo la coda del direttore
  fPtr = open_log(config.file);
  if(fPtr == NULL)
    return -1;
  pthread_t th_direttore;
  pthread_t th_cliente[config.C];
  pthread_t th_cassiere[config.K];

  CHECK_THREAD(pthread_create(&th_direttore, NULL, &fun_direttore, NULL), "error while creating thread (direttore)");
  for (size_t i = 0; i < config.K; i++)
    CHECK_THREAD(pthread_create(&th_cassiere[i], NULL, &fun_cassiere, (void*)i), "error while creating thread (cassiere)");
  for (size_t i = 0; i < config.C; i++)
    CHECK_THREAD(pthread_create(&th_cliente[i], NULL, &fun_cliente, (void*)i), "error while creating thread (cliente)");

  for (size_t i = 0; i < config.C; i++)      //aspetto che i primi C clienti terminino
    CHECK_THREAD(pthread_join(th_cliente[i], NULL), "error while joining (cliente)");

  //fin quando ci sono clienti all'interno del supermercato, devo attendere la terminazione di questi
  pthread_mutex_lock(&n_clientmtx);
  while(n_clienti > 0)
    pthread_cond_wait(&exitclient_wait, &n_clientmtx); //aspetta che i thread addizionali (quelli detached) terminino
  pthread_mutex_unlock(&n_clientmtx);
  //terminazione dei cassieri
  for (size_t i = 0; i < config.K; i++)
    CHECK_THREAD(pthread_join(th_cassiere[i], NULL), "error while joining (cassiere)");

  //pthread_cond_broadcast(&pending_wait);  //sveglia la lista di clienti in fila dal direttore nel caso stia aspettando che se ne inserisca uno
  CHECK_THREAD(pthread_join(th_direttore, NULL), "error while joining (direttore)");

  stampa_statistiche();
  free_memory();
  printf("Processo terminato\n");
  return 0;
}


void* fun_cliente(void* args){

    int i = (size_t)args;
    unsigned int seed = rand();
    coda nuovoCliente = malloc(sizeof(cliente));
    if(nuovoCliente == NULL){
      fprintf(stderr, "can not allocate memory for another client");
      return NULL;
    }
    nuovoCliente->next = NULL;
    nuovoCliente->index = i;
    nuovoCliente->n_prodotti = rand_r(&seed) % (config.P-1);   //[0-100]
    nuovoCliente->tempo_acquisti = 10 + (rand_r(&seed) % (config.T-9)); //[10-200]
    nuovoCliente->servito = 0;
    nuovoCliente->casse_cambiate = 0;
    if(pthread_cond_init(&nuovoCliente->turno, NULL) != 0){
      perror("error while initializing cond of client");
      free(nuovoCliente);
      pthread_mutex_lock(&n_clientmtx);
      n_clienti--;                      //diminuisco il numero clienti in modo sicuro
      pthread_mutex_unlock(&n_clientmtx);
      pthread_cond_signal(&exitclient_wait);
      return NULL;
    }
    //printf("Cliente %d || prodotti %d || tempo %d\n", i, nuovoCliente->n_prodotti, nuovoCliente->tempo_acquisti);
    nuovoCliente->t_attesa = 0;
    //inizializzo il tempo totale nel Supermercato
    clock_gettime(CLOCK_MONOTONIC, &nuovoCliente->t_start);
    //simulo il tempo che il cliente impiega a fare la spesa
    struct timespec tim;
    tim.tv_sec = (time_t)nuovoCliente->tempo_acquisti/1000;
    tim.tv_nsec = (time_t)(nuovoCliente->tempo_acquisti%1000)*1000000;
    nanosleep(&tim, NULL);  //aspetto che il cliente faccia la spesa

    if(!nuovoCliente->n_prodotti){ //se il cliente non ha acquistato prodotti, chiede al direttore se puo' uscire
      pthread_mutex_lock(&pending_access);
      if(!getQuit()){   //se il supermercato ha ricevuto SIGQUIT, fa uscire tutti i clienti all'interno
        enqueue_client(&pending_client, &nuovoCliente);
        pthread_cond_signal(&pending_wait);
        while(!nuovoCliente->servito)
          pthread_cond_wait(&nuovoCliente->turno, &pending_access);
      }
      pthread_mutex_unlock(&pending_access);
    }
    else{
      int r = randomizza_cassa();
      pthread_mutex_lock(&casse[r].mtx);    //piu' clienti non possono andare sulla stessa cassa contemporaneamente

      if(!getQuit()){  //se il supermercato ha ricevuto SIGQUIT, fa uscire tutti i clienti all'interno
        enqueue_client(&(casse[r].lista), &nuovoCliente);     //inserisci il cliente nella coda
        casse[r].n_elementi++;
        pthread_cond_signal(&(casse[r].empty)); //segnala che alla cassa [r] ci sara' almeno un cliente

        while(!nuovoCliente->servito){
          pthread_cond_wait(&nuovoCliente->turno, &casse[r].mtx);
        }
      }
      pthread_mutex_unlock(&(casse[r].mtx));
    }
    clock_gettime(CLOCK_MONOTONIC, &nuovoCliente->t_finish);
    float f = (float)(nuovoCliente->t_finish.tv_sec - nuovoCliente->t_start.tv_sec);
    f += (float)(nuovoCliente->t_finish.tv_nsec - nuovoCliente->t_start.tv_nsec) / 1000000000;

    nuovoCliente->t_tot = f;
    pthread_mutex_lock(&write_mtx);
    //printf("Cliente %d | %d | %.3fs | %.3fs | %d\n", nuovoCliente->index, nuovoCliente->n_prodotti, nuovoCliente->t_tot, nuovoCliente->t_attesa, nuovoCliente->casse_cambiate);
    fprintf(fPtr, "Cliente %d | %d | %.3fs | %.3fs | %d\n", nuovoCliente->index, nuovoCliente->n_prodotti, nuovoCliente->t_tot, nuovoCliente->t_attesa, nuovoCliente->casse_cambiate);
    pthread_mutex_unlock(&write_mtx);
    free(nuovoCliente);
    pthread_mutex_lock(&n_clientmtx);
    n_clienti--;                      //diminuisco il numero clienti in modo sicuro
    pthread_mutex_unlock(&n_clientmtx);
    pthread_cond_signal(&exitclient_wait);
    return NULL;
}

void* fun_cassiere(void* args){

  pthread_t th_comunicazione;
  CHECK_THREAD(pthread_create(&th_comunicazione, NULL, &timer, args), "error while creating timer thread");
  int i = (size_t)args;
  while(1){
    pthread_mutex_lock(&casse[i].mtx);
    while(!casse[i].aperta && !getQuit() && !getHup() && casse[i].lista == NULL){      //se il supermercato e' aperto e la cassa e' chiusa, attendo che quest'ultima apra o che il supermercato termini
      pthread_cond_wait(&casse[i].open, &casse[i].mtx);     //la condizione casse[i].lista == NULL serve perche' e' possibile che avendo n clienti in coda, possa spostarne n-1 in un'altra cassa.
    }                                                      //grazie a questa condizione aggiuntiva lo servo prima di chiudere la cassa
    while(casse[i].lista == NULL && !getHup() && !getQuit())    //la cassa aspetta che arrivi un cliente durante il periodo di apertura
      pthread_cond_wait(&(casse[i].empty), &casse[i].mtx);

    if(!getQuit() && !getHup() && casse[i].lista != NULL)      //il cassiere serve il primo cliente in coda
      pay_client(&casse[i]);

    if(getQuit() && casse[i].lista != NULL)     //se ci sono clienti in coda alla cassa e il supermercato riceve SIGQUIT, fa uscire in modo forzato tutti i clienti
      eject_client(&casse[i], i);

    if(casse[i].lista == NULL && ((getHup() && !getClient_n()) || getQuit())){      //se il supermercato chiude e non ci sono clienti alle casse posso far terminare il cassiere
      if(casse[i].aperta){
        clock_gettime(CLOCK_MONOTONIC, &casse[i].t_finish);
        float f = (float)(casse[i].t_finish.tv_sec - casse[i].t_start.tv_sec);
        f += (float)(casse[i].t_finish.tv_nsec - casse[i].t_start.tv_nsec) / 1000000000;
        add_data(&casse[i].t_aperture, f);
      }
      pthread_mutex_unlock(&casse[i].mtx);
      break;
    }
    while(getHup() && casse[i].lista != NULL)
      pay_client(&casse[i]);
    pthread_mutex_unlock(&casse[i].mtx);
  }
  CHECK_THREAD(pthread_join(th_comunicazione, NULL), "error while joining timer thread");
  return NULL;
}

void* timer(void* args){

  int i = (size_t)args;
  struct timespec tim;
  tim.tv_sec = (time_t)config.TIMER/1000;
  tim.tv_nsec = (time_t)(config.TIMER%1000)*1000000;
  pthread_mutex_t timer_mtx;
  pthread_mutex_init(&timer_mtx, NULL);
  while(!getQuit() && !getHup()){ //le casse possono comunicare col direttore solo se il supermercato e' aperto

    pthread_mutex_lock(&sendmtx);
    while(!casse[i].aperta && !getHup() && !getQuit()){ //se la cassa e' chiusa non posso inviare informazioni al direttore, aspetto
      pthread_cond_wait(&timer_wait[i], &sendmtx);
    }
    pthread_mutex_unlock(&sendmtx);

    if(getQuit() || getHup())
      return NULL;

    nanosleep(&tim, NULL);
    pthread_mutex_lock(&sendmtx);   //il direttore puo' "ascoltare" le info di un cassiere per volta
    send[i] = casse[i].n_elementi;  //salvo il numero di elementi in coda alla cassa in questo momento (il numero effettivo puo' varaire durante l'esecuzione: gestito in seguito)
    is_send = 1;    //segna che e' stato mandato un'informazione al direttore
    pthread_cond_signal(&sendwait);
    pthread_mutex_unlock(&sendmtx);
  }
  return NULL;
}

void* fun_direttore(void* args){
  int posti;  //indica il numero di clienti che possono entrare nel supermercato
  int exitvalue = 0;
  pthread_t th_fromClient;
  pthread_t th_fromCassa;
  CHECK_THREAD(pthread_create(&th_fromClient, NULL, &comunicazione_cliente, NULL), "error while creating thread (comunicazione cliente-direttore)");
  CHECK_THREAD(pthread_create(&th_fromCassa, NULL, &comunicazione_casse, NULL), "error while creating thread (comunicazione cliente-direttore)");
  while(!exitvalue){
    //gestisco l'uscita pulita delle casse
    if(getQuit() || getHup()){  //quando il supermercato chiude sveglio le casse precedentemente in attesa per farle uscire
      for (size_t i = 0; i < config.K; i++) {
        pthread_cond_signal(&casse[i].empty);
        pthread_cond_signal(&casse[i].open);
        pthread_cond_signal(&timer_wait[i]);
      }
      pthread_cond_signal(&sendwait);
      pthread_cond_signal(&pending_wait);
      exitvalue = 1;
    }
    //gestisco l'entrata dei nuovi E clienti
    posti = (config.C - getClient_n());
    if(posti >= config.E && !getHup() && !getQuit()){   //posso far entrare altri E clienti se il supermercato e' aperto
      pthread_t th_new[posti];
      for (size_t i = 0; i < posti; i++){
        pthread_mutex_lock(&n_clientmtx);
        n_clienti++;                      //incremento il numero clienti in modo sicuro
        pthread_mutex_unlock(&n_clientmtx);
        counter++;
        CHECK_THREAD(pthread_create(&th_new[i], NULL, &fun_cliente, (void*)counter), "error while creating new client");
        CHECK_THREAD(pthread_detach(th_new[i]), "error while setting a thread as detached");
      }
    }
  }
  CHECK_THREAD(pthread_join(th_fromCassa, NULL), "error while joining (comunicazione casse-direttore)");
  CHECK_THREAD(pthread_join(th_fromClient, NULL), "error while joining (comunicazione cliente-direttore)");
  pthread_mutex_lock(&pending_access);        //quando il supermercato riceve un segnale di terminazione
  for (size_t i = 0; i < config.K; i++) {     //puo' capitare che alla chiusura restino clienti in attesa dal direttore
    while(pending_client != NULL){            //e' necessario svegliarli tutti per poterli far uscire correttamente
      coda current = pending_client;
      clock_gettime(CLOCK_MONOTONIC, &current->t_finish);
      float t = (float)(current->t_finish.tv_sec - current->t_start.tv_sec);
      t += (float)(current->t_finish.tv_nsec - current->t_start.tv_nsec) / 1000000000.0;
      current->t_attesa = t;
      current->servito = 1;
      pthread_cond_signal(&current->turno);
      pending_client = pending_client->next;
    }
  }
  pthread_mutex_unlock(&pending_access);
  return NULL;
}

void* comunicazione_cliente(void* args){

  int exitvalue = 0;
  while(!exitvalue){
    pthread_mutex_lock(&pending_access);
    while(pending_client == NULL && !getQuit() && !getHup())
      pthread_cond_wait(&pending_wait, &pending_access);

    if(getQuit() || getHup())   //se il supermercato chiude, faccio terminare eventuali clienti in attesa
      exitvalue = 1;

    while(pending_client != NULL){
      coda current = pending_client;
      clock_gettime(CLOCK_MONOTONIC, &current->t_finish);
      float t = (float)(current->t_finish.tv_sec - current->t_start.tv_sec);
      t += (float)(current->t_finish.tv_nsec - current->t_start.tv_nsec) / 1000000000.0;
      current->t_attesa = t;
      current->servito = 1;
      pthread_cond_signal(&current->turno);
      pending_client = pending_client->next;
    }
    pthread_mutex_unlock(&pending_access);
  }
  return NULL;
}

void* comunicazione_casse(void* args){
  int ctr, x;
  while(!getHup() && !getQuit()){
    ctr = 0;
    pthread_mutex_lock(&sendmtx);
    while(!is_send)   //se ancora nessun cassiere ha mandato informazioni, il direttore le attende
      pthread_cond_wait(&sendwait, &sendmtx);

    for (size_t i = 0; i < config.K && !getHup() && !getQuit(); i++){    //se il supermercato e' aperto controlo quali casse posso aprire/chiudere
      if(send[i] > config.S2 && n_open < config.K){     //se una cassa ha piu' di S2 clienti in coda, ne apro una chiusa (se possibile)
        x = randomizza_cassa_chiusa();  //scelgo in modo casuale un indice di cassa chiusa
        pthread_mutex_lock(&casse[x].mtx);
        casse[x].aperta = 1;                //apro la cassa scelta in modo randomico
        clock_gettime(CLOCK_MONOTONIC, &casse[x].t_start);
        n_open++;
        pthread_cond_signal(&casse[x].open);
        pthread_cond_signal(&timer_wait[x]);
        pthread_mutex_unlock(&casse[x].mtx);
      }
      if(send[i] < 2 && send[i] != -1 && casse[i].aperta)   //se S1 casse (aperte) hanno al piu' un cliente in coda, ne chiudo una randomica
        ctr++;
      send[i] = -1;       //resetto la variabile per indicare che ho ricevuto l'informazione da parte della cassa i
    }
    pthread_mutex_unlock(&sendmtx);

    //gestisco l'informazione ricevuta
    if(ctr >= config.S1 && n_open > 1 && !getHup() && !getQuit()){ //se posso chiudere una cassa
      x = randomizza_cassa(); //scelgo in modo random una cassa aperta per poterla chiudere
      pthread_mutex_lock(&casse[x].mtx);
      coda scorri = casse[x].lista;
      while(scorri != NULL){
        scorri = scorri->next;
      }
      casse[x].aperta = 0;  //indico che la cassa e' chiusa
      move_client(x);   //sposto i clienti in una cassa aprta
      casse[x].n_chiusure++;
      clock_gettime(CLOCK_MONOTONIC, &casse[x].t_finish);
      float f = (float)(casse[x].t_finish.tv_sec - casse[x].t_start.tv_sec);
      f += (float)(casse[x].t_finish.tv_nsec - casse[x].t_start.tv_nsec) / 1000000000.0;
      add_data(&casse[x].t_aperture, f);
      n_open--;
      pthread_mutex_unlock(&casse[x].mtx);
    }
  }
  return NULL;
}

void quithandler(){
  quitval = 1;
}

void huphandler(){
  hupval = 1;
}

int getClient_n(){
  int i;
  pthread_mutex_lock(&n_clientmtx);
  i = n_clienti;
  pthread_mutex_unlock(&n_clientmtx);
  return i;
}

void free_memory(){
  if(fclose(fPtr) == EOF) //chiudo il file di log
    perror("error while closing log file");
  free(pending_client);
  free(send);
  free(casse);
}
