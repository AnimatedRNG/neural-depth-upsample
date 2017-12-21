CC=gcc
CFLAGS=-I.

fg: hooks.c
	$(CC) -shared -ldl -fPIC -lX11 -lGL -L./elfhacks/src -lelfhacks hooks.c -o hooks.so
