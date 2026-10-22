CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

.PHONY: all clean

all: generadorCSV

generadorCSV: generadorCSV.c
	$(CC) $(CFLAGS) generadorCSV.c -o generadorCSV



clean:
	rm -f generadorCSV