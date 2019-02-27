all: srv cli
	
srv: srv.c
	gcc -Wall -o srv srv.c mylib.c -lpthread -lmysqlclient

cli: cli.c
	gcc -Wall -o cli cli.c mylib.c -lpthread -lmysqlclient



