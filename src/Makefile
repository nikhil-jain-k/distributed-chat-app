CC=gcc
CFLAGS=-Wall -pedantic -pthread -std=gnu99
DEBUG=-g

all: client server clean

# Generate executables by linking object files
client: client.o checkargs.o errors.o parser.o servercommands.o
	$(CC) $(CFLAGS) $(DEBUG) -o client client.o checkargs.o errors.o parser.o servercommands.o

server: server.o checkargs.o errors.o parser.o
	$(CC) $(CFLAGS) $(DEBUG) -o server server.o checkargs.o errors.o parser.o

# Compile source files to objects
client.o: client.c
	$(CC) $(CFLAGS) $(DEBUG) -c client.c

server.o: server.c
	$(CC) $(CFLAGS) $(DEBUG) -c server.c

checkargs.o: checkargs.c
	$(CC) $(CFLAGS) $(DEBUG) -c checkargs.c

errors.o: errors.c
	$(CC) $(CFLAGS) $(DEBUG) -c errors.c

parser.o: parser.c
	$(CC) $(CFLAGS) $(DEBUG) -c parser.c

servercommands.o: servercommands.c
	$(CC) $(CFLAGS) $(DEBUG) -c servercommands.c

clean:
	rm -f *.o *~