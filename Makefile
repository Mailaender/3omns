all: 3omns
3omns: main.c
	$(CC) `sdl2-config --cflags` $^ `sdl2-config --libs` -l SDL2_image -o $@
