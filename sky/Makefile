CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDLIBS = -ldl -lm

host: host.c gl_min.h
	$(CC) $(CFLAGS) -o host host.c $(LDLIBS)

clean:
	rm -f host

.PHONY: clean
