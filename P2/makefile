CC=gcc
CFLAGS=-c
DEBUG=-g
DEPS_SERV_PROG=shmserver.o
DEPS_CLIENT_PROG=shmclient.o

all: prod_shmserver prod_shmclient shmserver shmclient

shmserver: $(DEPS_SERV_PROG)
	$(CC) $(DEPS_SERV_PROG) -o shmserver

shmclient: $(DEPS_CLIENT_PROG)
	$(CC) $(DEPS_CLIENT_PROG) -o shmclient

clean:
	rm -rf *.out shmserver shmclient *.o *.dSYM

shmserver.o:shmserver.c
	$(CC) $(CFLAGS) shmserver.c

shmclient.o:shmclient.c
	$(CC) $(CFLAGS) shmclient.c

debug: shmserver_debug shmclient_debug

shmserver_debug:
	$(CC) $(DEBUG) shmserver.c -o shmserver

shmclient_debug:
	$(CC) $(DEBUG) shmclient.c -o shmclient

prod_shmserver:
	cp shmserver.c tempserv.c
	cat copyright > shmserver.c
	cat tempserv.c >> shmserver.c
	rm tempserv.c

prod_shmclient:
	cp shmclient.c tempclient.c
	cat copyright > shmclient.c
	cat tempclient.c >> shmclient.c
	rm tempclient.c
# run:
# 	./HTTPServer 8080