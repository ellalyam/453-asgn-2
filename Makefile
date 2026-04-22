CC = gcc
CFLAGS = -Wall -I. -Werror

liblwp.so: lwp.c lwp.h fp.h magic64.o
	$(CC) $(CFLAGS) -shared -fpic -o liblwp.so lwp.c magic64.o

magic64.o: magic64.S
	$(CC) -o magic64.o -c magic64.S

clean:
	rm -f *.o *.so
