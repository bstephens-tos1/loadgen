CC = gcc
FLAGS = -std=c99 -Wall

all:
	$(CC) $(FLAGS) -o loadgen loadgen.c
debug:
	$(CC) $(FLAGS) -o loadgen loadgen.c -DDEBUG
clean:
	rm -f *.o loadgen
