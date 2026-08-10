CC = $(CROSS_COMPILE)gcc

aesdsocket: aesdsocket.c
	$(CC) $? -g -o $@

clean: aesdsocket
	rm $?

all: aesdsocket
default: aesdsocket