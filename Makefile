CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDLIBS = -lm

all: nav run

nav: src/nav.c
	$(CC) $(CFLAGS) -o nav src/nav.c $(LDLIBS)

run: nav
	mkdir -p out
	./nav data out

clean:
	rm -f nav out/sites_filled.csv out/index.html out/summary.txt

.PHONY: all run clean
