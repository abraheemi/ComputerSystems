TARGETS=server 

CFLAGS=-Wall -g

all: $(TARGETS)

server: server.c
	gcc $(CFLAGS) -o server server.c -pthread

clean:
	rm -f $(TARGETS)

