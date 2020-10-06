#!/bin/bash

if [ $# -ne 1 ]; then           #se il numero di argomenti passati e' diverso da 1
  echo "error: $(basename $0) must have an argument" 1>&2 #stampa un messaggio di errore sullo stderr
  exit -1                 # esce dallo script
fi

pid=($(pidof supermercato)) #salvo il pid del processo supermercato
while [ ! -z ${pid} ]; do   #se il pid tornato e' un campo nullo, vuol dire che il processo supermercato non e' in esecuzione
  sleep 1
  pid=($(pidof supermercato))  #salvo il pid del processo supermercato
done

if [ ! -f $1 ]; then            # se il file non esiste o non e' regolare
  echo "error: cant find log.txt" 1>&2    # stampa un messaggio di errore
  exit -1      # esce dallo script
fi

exec 3<$1
while read -ru 3 line; do

  if [ ${line%% *} = "Cassiere" ]; then          #stampo le statistiche dei cassieri
    cassiere=$(echo ${line} | cut -f1 -d"|")
    n_prodotti=$(echo ${line} | cut -f2 -d"|")
    n_clienti=$(echo ${line} | cut -f3 -d"|")
    n_chiusure=$(echo ${line} | cut -f4 -d"|")
    times=($(echo ${line} | cut -f5 -d"|"))    #tempo totale di apertura
    sum=0
    count=${#times[@]}
    for ((i=0; i<count; i+=1)); do
      current=${times[${i}]}
      sum=$(echo "$current + $sum" | bc -l)
    done
    #tempo medio di servizio
    times=($(echo ${line} | cut -f6 -d"|"))
    avg=0
    count=${#times[@]}
    if [ $count -ne 0 ]; then
      for ((i=0; i<count; i+=1)); do
        current=${times[${i}]}
        avg=$(echo "scale=3; $current + $avg" | bc -l)
      done
      avg=$(echo "scale=3; $avg / $count" | bc -l)
    fi
    echo -e "${cassiere}\t| ${n_prodotti} \t| ${n_clienti}\t| ${n_chiusure}\t| ${sum}\t| ${avg}"
  elif [ ${line%% *} = "Cliente" ]; then    #stampo le statistiche dei clienti
    cliente=$(echo ${line} | cut -f1 -d"|")
    prodotti=$(echo ${line} | cut -f2 -d"|")
    t_tot=$(echo ${line} | cut -f3 -d"|")
    t_coda=$(echo ${line} | cut -f4 -d"|")
    n_code=$(echo ${line} | cut -f5 -d"|")
    n_code=$(echo "${n_code} + 1" | bc -l )
    echo -e "${cliente}\t| ${prodotti}\t| ${t_tot}\t| ${t_coda}\t| ${n_code}"
  else #stampo le statistiche del supermercato
    echo ${line}
  fi
done
exec 3<&- #chiudo il file del file descriptor 3
