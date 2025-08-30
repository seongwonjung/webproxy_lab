CC = gcc
CFLAGS = -O2 -Wall -Wextra -pthread
CSAPP = ../csapp.c

all: echoserveri echoclient

echoserveri: echoserveri.o echo.o $(CSAPP:.c=.o)
	$(CC) $(CFLAGS) -o $@ echoserveri.o echo.o $(CSAPP:.c=.o)

echoclient: echoclient.o $(CSAPP:.c=.o)
	$(CC) $(CFLAGS) -o $@ echoclient.o $(CSAPP:.c=.o)

%.o: %.c ../csapp.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o echoserveri echoclient
