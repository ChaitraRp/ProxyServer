CC=gcc
CFLAGS= 


SRC= proxyserver.c
 

all: $(SRC)
	$(CC) -o webproxy.o $(SRC)

.PHONY : clean
clean :
	-rm -f *.o .cache
