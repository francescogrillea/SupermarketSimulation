#include <stdio.h>
#include <stdlib.h>

#define CHECK_THREAD(r,s)\
  if(r != 0){\
    perror(s);\
    _exit(EXIT_FAILURE);}

#define CHECK_ZERO(r,s)\
  if(r != 0){\
    perror(s);\
    _exit(EXIT_FAILURE);}
