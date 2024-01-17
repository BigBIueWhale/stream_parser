CC=gcc
CFLAGS=-std=gnu11 -Wall -Wextra -pedantic
DFLAGS=-g
RFLAGS=-O2

.PHONY: all clean debug release

# Default to release mode
all: release

debug: CFLAGS += $(DFLAGS)
debug: stream_parser

release: CFLAGS += $(RFLAGS)
release: stream_parser

stream_parser: main.o stream_parser.o crc32.o
	$(CC) $(CFLAGS) -o stream_parser main.o stream_parser.o crc32.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

stream_parser.o: stream_parser.c
	$(CC) $(CFLAGS) -c stream_parser.c

crc32.o: crc32.c
	$(CC) $(CFLAGS) -c crc32.c

clean:
	rm -f *.o stream_parser
