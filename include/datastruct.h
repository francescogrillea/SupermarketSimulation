#include <pthread.h>

//struttura che permette di salvare il tempo di servizio per ogni cliente
typedef struct n{
  float t;
  struct n* next;
}delta_t;
typedef delta_t* time_list;

//struttura di un cliente
typedef struct c{
  int index;                      //id cliente
  int n_prodotti;                 //numero di prodotti acquistati (randomico)
  int tempo_acquisti;             //tempo impiegato per fare acquisti (randomico)
  float t_tot;                    //tempo totale che il cliente spende all'interno del supermercato
  struct timespec t_start;        //tempo in cui il cliente si mette in coda
  struct timespec t_finish;       //tempo in cui il cliente viene servito
  float t_attesa;                 //tempo in cui un cliente aspetta il suo turno in coda ad una cassa
  int servito;                    //variabile d'appoggio che indica se il cliente e' stato servito
  int casse_cambiate;           //numero di casse cambiate
  pthread_cond_t turno;         //il cliente aspetta il proprio turno
  struct c* next;
}cliente;
typedef cliente* coda;          //coda di clienti da "attaccare" ad ogni cassa

//struttura di un cassiere
typedef struct {
  int n_elementi;
  coda lista;
  int n_prodotti_totali;
  int n_clienti_serviti;
  int tempo_cassiere;
  int n_chiusure;
  int aperta;
  struct timespec t_start;        //tempo di inizio di un turno di apertura
  struct timespec t_finish;       //tempo di fine di un turno di apertura
  time_list t_cliente;            //tempo di servizio di ogni cliente gestito dal cassiere
  time_list t_aperture;           //tempo di ogni apertura della cassa
  pthread_mutex_t mtx;            //non possono accodarsi due clienti nella stessa cassa contemporaneamente
  pthread_cond_t empty;           //la cassa aspetta che ci siano clienti per poterli servire
  pthread_cond_t open;            //la cassa e' aperta
}cassiere;

int crea_casse();
int getQuit();        //resituisce 1 se e' stato dato SIGQUIT, 0 altrimenti
int getHup();         //resituisce 1 se e' stato dato SIGHUP, 0 altrimenti
void enqueue_client(coda* fila, coda* add);    //un cliente si mette in coda ad una cassa
void pay_client(cassiere* c); //un cliente viene servito dal cassiere
void eject_client(cassiere* c, int i);    //se il supermercato chiude, tutti i clienti in coda alla cassa vengono fatti uscire
void move_client(int x);  //sposto i clienti in coda da una cassa che dev'essere chiusa ad un cassa aperta
int randomizza_cassa();
int randomizza_cassa_chiusa();
void add_data(time_list* l, float x);
void stampa_statistiche();  //stampa le statistiche dei cassieri e del supermercato
