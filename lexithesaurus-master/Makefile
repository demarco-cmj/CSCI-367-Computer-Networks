CC=gcc
PORT=36799
SIZE=8
SECONDS=30
FILE=twl06.txt

prog2_client: prog2_client.o prog2_server.o
	$(CC) -o prog2_client prog2_client.o
	$(CC) -o prog2_server prog2_server.o trie.c

runc: prog2_client
	./prog2_client 127.0.0.1 $(PORT)

runs: prog2_server 
	./prog2_server $(PORT) $(SIZE) $(SECONDS) $(FILE)

clean:
	rm -r prog2_client prog2_client.o
	rm -r prog2_server prog2_server.o