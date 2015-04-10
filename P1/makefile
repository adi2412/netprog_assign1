CC=gcc
CFLAGS=-c
DEBUG=-g
DEPS_SHELL_PROG=shell.o

all: prod_shell

shell: $(DEPS_SHELL_PROG)
	$(CC) $(DEPS_SHELL_PROG) -o shell

clean:
	rm -rf *.out shell *.o *.dSYM

shell.o:shell.c
	$(CC) $(CFLAGS) shell.c

debug: shell_debug

shell_debug:
	$(CC) $(DEBUG) shell.c -o shell

prod_shell:
	cp shell.c tempserv.c
	cat copyright > shell.c
	cat tempserv.c >> shell.c
	rm tempserv.c

# run:
# 	./HTTPServer 8080