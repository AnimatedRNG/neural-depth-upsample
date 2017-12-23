CC=gcc
CFLAGS=-I.

fg: hooks.c
	$(CC) -Iminiz/ -shared -ldl -fPIC -g -lX11 -lGL -L./elfhacks/src -lelfhacks miniz/amalgamation/miniz.c capture_pbo.c hooks.c -o hooks.so
