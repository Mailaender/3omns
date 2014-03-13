all: 3omns
3omns: main.c util.c sdl/sdl.c
	$(CC) -Wall `sdl2-config --cflags` -I. -std=c11 $^ `sdl2-config --libs` -l SDL2_image -o $@
