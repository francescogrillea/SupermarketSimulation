CC				=  gcc
CFLAGS		+= -std=c99 -Wall
AR        =  ar
ARFLAGS   =  rvs

INCDIR		= ./include/
LIBDIR		= ./lib/
SRCDIR    = ./src/
BINDIR    = ./bin/

INCLUDES	= -I $(INCDIR)
LIBS      = -ldata -lconfig -lpthread
LDFLAGS 	= -L $(LIBDIR)
OPTFLAGS	= -g -O3

TARGETS 	= $(BINDIR)supermercato

.PHONY: all clean test
.SUFFIXES: .c .h

$(BINDIR)%: $(SRCDIR)%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

#creo tutti i file oggetto
$(SRCDIR)%.o: $(SRCDIR)%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all	:	$(TARGETS)
	chmod +x analisi.sh

#creo l'eseguibile tramite la seguente dependency list
$(TARGETS): $(SRCDIR)supermercato.c $(LIBDIR)libdata.a $(LIBDIR)libconfig.a

#se le dipendenze non esistono vengono create

$(LIBDIR)libdata.a: $(SRCDIR)datastruct.o
	$(AR) $(ARFLAGS) $@ $<

$(LIBDIR)libconfig.a: $(SRCDIR)config.o
	$(AR) $(ARFLAGS) $@ $<

clean	:
	rm -f $(TARGETS) log.txt
	find . \( -name *.a -o -name *.o \) -exec rm -f {} \;
	chmod -x analisi.sh

test	:
	$(TARGETS) config.txt & sleep 25
	killall -1 $(TARGETS)
	./analisi.sh log.txt
