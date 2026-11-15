CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDLIBS = -lm

all: nav run sky

nav: src/nav.c
	$(CC) $(CFLAGS) -o nav src/nav.c $(LDLIBS)

run: nav
	mkdir -p out
	./nav data out

# 3D sky domes: nav writes out/sky/*.txt, the OpenGL host renders each
# site to out/sky/*.png, then nav runs again to embed the renders in
# the page. The sky PNGs are build outputs (see .gitignore); the page
# carries them base64, so it stays a single file.
sky: run
	./sky/render.sh
	./nav data out

clean:
	rm -f nav out/sites_filled.csv out/index.html out/summary.txt
	rm -rf out/sky

.PHONY: all run sky clean
