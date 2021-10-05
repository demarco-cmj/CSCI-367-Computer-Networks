CC=gcc
PORT1=40000
PORT2=40001

prog3_server: prog3_server.o prog3_participant.o prog3_observer.o 
	$(CC) -o prog3_server prog3_server.o
	$(CC) -o prog3_participant prog3_participant.o
	$(CC) -o prog3_observer prog3_observer.o

runp: prog3_participant
	./prog3_participant 127.0.0.1 $(PORT1)

runo: prog3_observer
	./prog3_observer 127.0.0.1 $(PORT2)

runs: prog3_server 
	./prog3_server $(PORT1) $(PORT2)

clean:
	rm -r prog3_participant prog3_participant.o
	rm -r prog3_observer prog3_observer.o
	rm -r prog3_server prog3_server.o