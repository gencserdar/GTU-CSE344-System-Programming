CC = gcc
CFLAGS = -Wall
SERVER_DIR = server_directory
CLIENTS = 2
T =
PID = 
.PHONY: all clean server client

all:

server: server.c
	$(CC) $(CFLAGS) -o $@ $^
	./server $(SERVER_DIR) $(CLIENTS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $^
	./client $(T) $(PID)

clean:
	rm -f server client *.log *.fifo
	rm -r $(SERVER_DIR)
