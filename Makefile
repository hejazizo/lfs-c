CC=gcc
CFLAGS=-Wall -lm -I. `pkg-config fuse --cflags`
LIBS=-lm `pkg-config fuse --libs`

all: mklfs test

mklfs: mklfs.o log.o flash.o directory.o file.o
	$(CC) $(CFLAGgdS) -o mklfs mklfs.o log.o flash.o directory.o file.o

test: test.o log.o flash.o file.o directory.o
	$(CC) $(CFLAGS) -o test test.o log.o flash.o file.o directory.o

mklfs.o: mklfs.c
	$(CC) $(CFLAGS) -c mklfs.c

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

log.o: log.c
	$(CC) $(CFLAGS) -c log.c

flash.o: flash.c
	$(CC) $(CFLAGS) -c flash.c

file.o: file.c
	$(CC) $(CFLAGS) -c file.c

directory.o: directory.c
	$(CC) $(CFLAGS) -c directory.c

.PHONY: clean

clean:
	rm -f *.o lfs lfsck mklfs test
