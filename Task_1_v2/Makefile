CC=gcc
LIBS=-lpthread -lmysqlclient
FLAGS=-Wall

all: srv cli
	
srv: srv.o mylib.o
	$(CC) $(FLAGS) mylib.o srv.o $(LIBS) -o srv

cli: cli.o mylib.o
	$(CC) $(FLAGS) mylib.o cli.o $(LIBS) -o cli

%.o: %.c
	$(CC) -c $^ -o $@

clean:
	rm -rf *.o srv cli
