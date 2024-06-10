CC = gcc
CFLAGS = -Wall

.PHONY: all clean

all: main
	./main

main: main.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f main fifo1 fifo2
