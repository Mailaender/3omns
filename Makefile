all: 3omns
3omns: main.c
	$(CC) -I /usr/local/include $^ -L /usr/local/lib -l SDL2 -Wl,-rpath=/usr/local/lib -o $@
