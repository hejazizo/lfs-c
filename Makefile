CC=gcc
CFLAGS=-Wall -lm -I. `pkg-config fuse --cflags`
LIBS=-lm `pkg-config fuse --libs`

all: lfs lfsck mklfs test

lfs: main_fuse.o log.o file.o directory.o flash.o
	$(CC) $(CFLAGS) $(LIBS) -o lfs main_fuse.o log.o file.o directory.o flash.o

lfsck: lfsck.o log.o file.o directory.o flash.o
	$(CC) $(CFLAGS) -o lfsck lfsck.o log.o file.o directory.o flash.o

mklfs: mklfs.o log.o flash.o directory.o file.o
	$(CC) $(CFLAGgdS) -o mklfs mklfs.o log.o flash.o directory.o file.o

test: test.o log.o flash.o file.o directory.o
	$(CC) $(CFLAGS) -o test test.o log.o flash.o file.o directory.o

main_fuse.o: main_fuse.c
	$(CC) $(CFLAGS) $(LIBS) -c main_fuse.c

lfsck.o: lfsck.c
	$(CC) $(CFLAGS) -c lfsck.c

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
