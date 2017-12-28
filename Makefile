CC=gcc
CFLAGS=-I.

fg: hooks.c
	$(CC) -Ipipe/ -Iminiz/ -Ielfhacks/src/ -shared -ldl -fPIC -g -pthread -lX11 -lGL -L./elfhacks/src -lelfhacks pipe/pipe.c miniz/amalgamation/miniz.c capture_pbo.c hooks.c -o hooks.so
