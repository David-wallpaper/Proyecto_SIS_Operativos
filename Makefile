CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

.PHONY: all clean

all: generadorCSV agentePais servidorISE

generadorCSV: generadorCSV.c
	$(CC) $(CFLAGS) generadorCSV.c -o generadorCSV

agentePais: agentePais.c
	$(CC) $(CFLAGS) agentePais.c -o agentePais

servidorISE: servidorISE.c
	$(CC) $(CFLAGS) -pthread servidorISE.c -o servidorISE

clean:
	rm -f generadorCSV agentePais servidorISE