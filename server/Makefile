CC = $(CROSS_COMPILE)gcc

aesdsocket: aesdsocket.c
	$(CC) $? -o $@

clean: aesdsocket
	rm $?

all: aesdsocket
default: aesdsocket