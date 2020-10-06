#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <datastruct.h>
#include <config.h>
#include <pthread.h>
#include <signal.h>

extern volatile sig_atomic_t quitval;   //flag binario che indica se il supermercato ha ricevuto SIGQUIT
static pthread_mutex_t quitmtx = PTHREAD_MUTEX_INITIALIZER;    //accedo a quitval in mutua esclusione
extern volatile sig_atomic_t hupval;   //flag binario che indica se il supermercato ha ricevuto SIGHUP
static pthread_mutex_t hupmtx = PTHREAD_MUTEX_INITIALIZER;     //accedo a hupval in mutua esclusione

extern configurazione config;  //parametri di configurazione definiti nel file config.txt
//extern unsigned int seed;

extern cassiere* casse;
extern int* send;
extern pthread_cond_t* timer_wait;
extern FILE* fPtr;
extern pthread_mutex_t write_mtx;
extern int n_open;

static int tot_clienti_serviti = 0; //contatore che indica il numero totale di clienti serviti
static int n_acquisti_totali = 0;  //contatore che indica il totale dei prodotti acquistati da tutti i clienti che sono stati serviti
static pthread_mutex_t update_value = PTHREAD_MUTEX_INITIALIZER;  //permette l'aggiornamento delle statistiche del supermercato in modo sicuro


int crea_casse(){
  unsigned int seed = rand();
  casse = malloc(config.K * sizeof(cassiere));
  if(casse == NULL){
    fprintf(stderr, "error on malloc");
    return 0;
  }
  send = malloc(sizeof(int) * config.K);
  if(send == NULL){
    fprintf(stderr, "error on malloc");
    return 0;
  }
  timer_wait = malloc(sizeof(pthread_cond_t) * config.K);
  for (size_t i = 0; i < config.K; i++){
    send[i] = -1;
    pthread_cond_init(&timer_wait[i], NULL);
    if(pthread_mutex_init(&casse[i].mtx, NULL) != 0){
      perror("error while initializing mutex (cassiere)");
      return 0;
    }
    if(pthread_cond_init(&casse[i].empty, NULL) != 0){
      perror("error while initializing cond (cassiere)");
      return 0;
    }
    if(pthread_cond_init(&casse[i].open, NULL) != 0){
      perror("errore while initializing cond (cassiere)");
      return 0;
    }
    casse[i].tempo_cassiere = 20 + rand_r(&seed) % 60;
    casse[i].n_elementi = 0;
    casse[i].lista = NULL;
    casse[i].aperta = rand_r(&seed) % 2;  //il supermercato sceglie in modo randomico quali casse aprire all'inizio
    if(i == 0 || casse[i].aperta){
      casse[i].aperta = 1;
      clock_gettime(CLOCK_MONOTONIC, &casse[i].t_start);
      n_open++;
    }
    casse[i].n_clienti_serviti = 0;
    casse[i].n_prodotti_totali = 0;
    casse[i].n_chiusure = 0;
    casse[i].t_cliente = NULL;
    casse[i].t_aperture = NULL;
  }
  return 1;
}

int getQuit(){
  int i;
  pthread_mutex_lock(&quitmtx);
  i = quitval;
  pthread_mutex_unlock(&quitmtx);
  return i;
}

int getHup(){
  int i;
  pthread_mutex_lock(&hupmtx);
  i = hupval;
  pthread_mutex_unlock(&hupmtx);
  return i;
}

void enqueue_client(coda* fila, coda* add){
  coda precedente = NULL;
  coda corrente = *fila;
  while(corrente != NULL){
    precedente = corrente;
    corrente = corrente->next;
  }
  if(precedente == NULL){
    (*add)->next = *fila;
    *fila = *add;
  }
  else{
    precedente->next = *add;
    (*add)->next = corrente;
  }
}

void pay_client(cassiere* c){
  coda current = c->lista;
  clock_gettime(CLOCK_MONOTONIC, &current->t_finish);
  float t = (float)(current->t_finish.tv_sec - current->t_start.tv_sec);
  t += (float)(current->t_finish.tv_nsec - current->t_start.tv_nsec) / 1000000000.0;
  current->t_attesa = t;
  int time = (c->tempo_cassiere + config.T_ELEM * current->n_prodotti);
  struct timespec tim;
  tim.tv_sec = (time_t)time/1000;
  tim.tv_nsec = (time_t)((time%1000)*1000000);
  nanosleep(&tim, NULL);  //aspetto che il cassiere serva il cliente
  c->n_elementi--;
  add_data(&(c->t_cliente), (float)time/1000);
  pthread_mutex_lock(&update_value);    //aggiorno in sicurezza i valori delle statistiche del supermercato
  c->n_prodotti_totali += current->n_prodotti;
  n_acquisti_totali += current->n_prodotti;
  tot_clienti_serviti++;
  pthread_mutex_unlock(&update_value);
  c->n_clienti_serviti++;
  current->servito = 1;
  pthread_cond_signal(&current->turno);
  c->lista = (c->lista)->next;
}

void eject_client(cassiere* c, int i){
  while(c->lista != NULL){
    coda current = c->lista;
    current->servito = 1;
    current->t_attesa = 0.000;
    pthread_cond_signal(&current->turno);
    c->lista = (c->lista)->next;
    c->n_elementi--;
  }
}

void move_client(int x){
  unsigned int seed = rand();
  if(casse[x].lista == NULL)   //e' possibile che il cassiere abbia servito tutti i clienti, quindi non ci sono clienti da spostare
    return;

  int dest;
  while((casse[x].lista)->next != NULL && !getQuit() && !getHup()){
    coda scorri = (casse[x].lista)->next;
    while(scorri != NULL)
      scorri = scorri->next;
    coda to_move;
    do{
      dest = rand_r(&seed) % config.K;
      while(dest == x)
        dest = rand_r(&seed) % config.K;
    }while(!casse[dest].aperta && !getQuit() && !getHup());
    pthread_mutex_lock(&casse[dest].mtx);
    to_move = (casse[x].lista)->next;
    to_move->casse_cambiate++;
    if(!getQuit() && !getHup()){
      (casse[x].lista)->next = (casse[x].lista)->next->next;
      enqueue_client(&(casse[dest].lista), &to_move);
      casse[dest].n_elementi++;
      casse[x].n_elementi--;
      pthread_cond_signal(&casse[dest].empty);
    }
    pthread_mutex_unlock(&casse[dest].mtx);
  }
}

int randomizza_cassa(){
  int r;
  unsigned int seed = rand();
  while(1){
    r = rand_r(&seed) % config.K;
    pthread_mutex_lock(&casse[r].mtx);
    if(casse[r].aperta){
      pthread_mutex_unlock(&casse[r].mtx);
      return r;
    }
    pthread_mutex_unlock(&casse[r].mtx);
  }
}

int randomizza_cassa_chiusa(){
  int r;
  unsigned int seed = rand();
  while(1){
    r = rand_r(&seed) % config.K;
    pthread_mutex_lock(&casse[r].mtx);
    if(!casse[r].aperta){
      pthread_mutex_unlock(&casse[r].mtx);
      return r;
    }
    pthread_mutex_unlock(&casse[r].mtx);
  }
}

void add_data(time_list* l, float x){
  time_list new = malloc(sizeof(delta_t));
  if(new == NULL){
    fprintf(stderr, "error while while creating data");
    //_exit(EXIT_FAILURE);
  }
  new->t = x;
  new->next = *l;
  *l = new;
}

void stampa_statistiche(){

  pthread_mutex_lock(&write_mtx);
  for (size_t i = 0; i < config.K; i++) {
    pthread_mutex_lock(&casse[i].mtx);
    fprintf(fPtr, "Cassiere %ld | %d | %d | %d | ", i, casse[i].n_prodotti_totali, casse[i].n_clienti_serviti, casse[i].n_chiusure);
    while(casse[i].t_aperture != NULL){
      time_list tmp = casse[i].t_aperture;
      casse[i].t_aperture = (casse[i].t_aperture)->next;
      fprintf(fPtr, "%.3f ", tmp->t);
      free(tmp);
    }
    free(casse[i].t_aperture);
    fprintf(fPtr, " | ");
    while(casse[i].t_cliente != NULL){
      time_list tmp = casse[i].t_cliente;
      casse[i].t_cliente = (casse[i].t_cliente)->next;
      fprintf(fPtr, "%.3f ", tmp->t);
      free(tmp);
    }
    fprintf(fPtr, "|\n");
    free(casse[i].t_cliente);
    pthread_mutex_unlock(&casse[i].mtx);
  }
  fprintf(fPtr, "Supermercato %d clienti totali serviti | %d prodotti totali acquistati\n", tot_clienti_serviti, n_acquisti_totali);
  pthread_mutex_unlock(&write_mtx);
}
