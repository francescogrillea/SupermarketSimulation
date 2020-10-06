typedef struct{
  int return_value;
  int K;            //# max di casse attive
  int C;            //# max di clienti nel supermercato
  int E;            //# min di clienti che devono uscire prima di farne entrare altri
  int T;            //tempo che ogni cliente passa da quando entra a quando va a pagare
  int P;            //#max di prodotti che il cliente puo' acquistare
  int T_ELEM;       //tempo fisso che ciascun elemento impiega per passare alla cassa
  int S1;   //# max di persone che possono stare in fila alla cassa
  int S2;   //# min di persone che possono stare in fila alla cassa
  int TIMER;
  char file[128];
}configurazione;

configurazione read_config(const char* file);
FILE* open_log(const char* file);
