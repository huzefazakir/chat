CC = gcc
CFLAGS = -Wall -g

all: client server

client: client.o
	$(CC) -o client client.o

server: server.o
	$(CC) -o server server.o

client.o: chat_client.c
	$(CC) $(CFLAGS) -c chat_client.c -o client.o

server.o: chat_server.c
	$(CC) $(CFLAGS) -c chat_server.c -o server.o

clean:
	rm *.o client server

