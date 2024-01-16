CC=gcc
CFLAGS=-std=gnu11 -Wall -Wextra -pedantic

.PHONY: all clean

all: stream_parser

stream_parser: main.o stream_parser.o
	$(CC) $(CFLAGS) -o stream_parser main.o stream_parser.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

stream_parser.o: stream_parser.c
	$(CC) $(CFLAGS) -c stream_parser.c

clean:
	rm -f *.o stream_parser
