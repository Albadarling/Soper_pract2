CC = gcc
CFLAGS = -lpthread -lrt

all : voting

voting.o: voting.c voting.h
	$(CC) -c voting.c $(CFLAGS)

voting: voting.o
	$(CC) voting.o -o voting $(CFLAGS)

clear:
	rm -rf *.o 

clean:
	rm -rf *.o voting
