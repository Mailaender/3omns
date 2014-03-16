all: 3omns
3omns: b3/b3.h main.c b3/sprite.c b3/util.c b3/sdl/sdl.c
	$(CC) -Wall `sdl2-config --cflags` -I . -I b3 -std=c11 $^ `sdl2-config --libs` -l SDL2_image -o $@
