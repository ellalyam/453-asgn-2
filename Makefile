CC = gcc
CFLAGS = -Wall -I.

all: test liblwp.so

# Your test program
test: test.c lwp.c magic64.o
	$(CC) $(CFLAGS) -o test test.c lwp.c magic64.o

# The shared library (what you'll submit)
liblwp.so: lwp.c magic64.o
	$(CC) $(CFLAGS) -shared -fpic -o liblwp.so lwp.c magic64.o

# Assemble the magic file
magic64.o: magic64.S
	$(CC) -o magic64.o -c magic64.S

clean:
	rm -f test liblwp.so magic64.o
	