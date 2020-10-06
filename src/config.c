#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <config.h>

configurazione read_config(const char* file){

  FILE* fPtr;
  fPtr = fopen(file, "r");
  configurazione struttura_config;
  char flag[32];
  char value[128];
  int err;
  while(!feof(fPtr)){
    err = fscanf(fPtr, " %s %s", flag, value);
    if(err != 2 && err != -1){
      struttura_config.return_value = 0;
      fclose(fPtr);
      return struttura_config;
    }
    struttura_config.return_value = 1;
    if(strcmp(flag,"K")==0){struttura_config.K = atoi(value);}
    else if(strcmp(flag,"C")==0){struttura_config.C = atoi(value);}
    else if(strcmp(flag,"E")==0){struttura_config.E = atoi(value);}
    else if(strcmp(flag,"T")==0){struttura_config.T = atoi(value);}
    else if(strcmp(flag,"P")==0){struttura_config.P = atoi(value);}
    else if(strcmp(flag,"T_ELEM")==0){struttura_config.T_ELEM = atoi(value);}
    else if(strcmp(flag,"S1")==0){struttura_config.S1 = atoi(value);}
    else if(strcmp(flag,"S2")==0){struttura_config.S2 = atoi(value);}
    else if(strcmp(flag,"TIMER")==0){struttura_config.TIMER = atoi(value);}
    else if(strcmp(flag,"LOGFILE")==0){strcpy(struttura_config.file, value);}
    else{   //perror exit(EXIT_FAILURE);
      struttura_config.return_value = 0;
      fclose(fPtr);
      return struttura_config;
    }
  }
  if(fclose(fPtr) == EOF){
    perror("error while closing config file");
    struttura_config.return_value = 0;
    return struttura_config;
  }
  return struttura_config;
}

FILE* open_log(const char* file){
  FILE* fPtr;
  if((fPtr = fopen(file, "w")) == NULL){
    perror("error while opening log file");
    return NULL;
  }
  return fPtr;
}
