CC = gcc
CFLAGS = -Wall -Wextra -pthread
SRCS = 210104004023_main.c manager.c worker.c buffer.c
OBJS = $(SRCS:.c=.o)
TARGET = MWCp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET)
	rm -f *.o
