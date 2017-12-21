CC=gcc
CFLAGS=-I.

fg: hooks.c
	$(CC) -shared -ldl -fPIC -g -lX11 -lGL -L./elfhacks/src -lelfhacks lodepng/lodepng.c capture_framebuffer.c hooks.c -o hooks.so
