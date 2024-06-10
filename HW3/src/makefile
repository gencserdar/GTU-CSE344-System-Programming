CC = gcc
CFLAGS = -Wall
.PHONY: all clean main

all: clean main

main: main.c
	$(CC) $(CFLAGS) -o $@ $^
	./main

clean:
	rm -f main
