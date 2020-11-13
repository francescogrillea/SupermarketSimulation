# Supermercato- Progetto di Sistemi Operativi

UniPi- Informatica  
Laboratorio di Sistemi Operativi- Progetto a.a. 2019-2020  

Programma multithreaded che simula un supermercato.   
Le entità principali sono Clienti, Cassieri e Direttore, rappresentati da thread: questi devono essere in grado di scambiarsi informazioni quali, ad esempio, il numero di casse aperte, il numero di clienti all'interno del supermercato ecc e operare in base ai valori passati come prametro.  
Il processo termina quando, dopo 25 secondi, verrà inviato il segnale SIGHUP. A questo punto si attende l'uscita di eventuali clienti ancora all'interno del supermercato per poter far terminare successivamente i cassieri e infine il direttore.
Per maggiori informazioni riguardo il testo consultare il file [specifiche.pdf](https://github.com/Osiride-projects/supermarket_simulation/blob/main/specifiche.pdf)  

## How to use  
**Install**
```bash
make
```  
**Run**
```bash
make test
```
**Unistall**
```bash
make clean
```

  
**ISSUES**  
Il thread direttore aspetta in attesa attiva utilizzando così un core al 100%  
  
## Valutazione del progeto  
26/30
