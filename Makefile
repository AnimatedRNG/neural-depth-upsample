CC=gcc
CFLAGS=-I.

fg: frame_grabber.c
	$(CC) -shared -ldl -fPIC -lX11 -lGL -L./elfhacks/src -lelfhacks frame_grabber.c -o frame_grabber.so
