CC=gcc
CFLAGS=-c
DEBUG=-g
DEPS_SERV_PROG=rshmServer.o
DEPS_CLIENT_PROG=client.o rshmAPI.o

all: prod_rshmServer prod_client rshmServer client

rshmServer: $(DEPS_SERV_PROG)
	$(CC) $(DEPS_SERV_PROG) -o rshmServer

client: $(DEPS_CLIENT_PROG)
	$(CC) $(DEPS_CLIENT_PROG) -o client

clean:
	rm -rf *.out rshmServer client *.o *.dSYM

rshmServer.o:rshmServer.c
	$(CC) $(CFLAGS) rshmServer.c

client.o:client.c
	$(CC) $(CFLAGS) client.c

rshmAPI.o:rshmAPI.c
	$(CC) $(CFLAGS) rshmAPI.c

debug: rshmServer_debug client_debug

rshmServer_debug:
	$(CC) $(DEBUG) rshmServer.c -o rshmServer

client_debug:
	$(CC) $(DEBUG) client.c rshmAPI.c -o client

prod_rshmServer:
	cp rshmServer.c tempserv.c
	cat copyright > rshmServer.c
	cat tempserv.c >> rshmServer.c
	rm tempserv.c

prod_client:
	cp client.c tempclient.c
	cat copyright > client.c
	cat tempclient.c >> client.c
	rm tempclient.c
# run:
# 	./HTTPServer 8080