# Supermercato- Progetto di Sistemi Operativi

UniPi- Informatica  
Laboratorio di Sistemi Operativi  
Progetto a.a. 2019-2020  
Sviluppato per Linux  
  
## Description  
  Programma multithreaded che simula un supermercato.   
  Le entità principali sono thread che rappresentano Clienti, Cassieri e Direttore. Questi devono essere in grado di scambiarsi informazioni concorrentemente.  
  Per maggiori informazioni riguardo il testo consultare il file specifiche.pdf  

## How to use  
Install
```bash
make
```  
Run
```bash
make test
```


  
**ISSUES**  
Il thread direttore aspetta in attesa attiva utilizzando così un core al 100%  
  
## Valutazione del progeto  
26/30
