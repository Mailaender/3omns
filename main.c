#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>


#define b3_fatal(format, ...) \
        b3_fatal_(__FILE__, __LINE__, __func__, format, __VA_ARGS__)
#define b3_sdl_fatal(failed) b3_fatal(failed ": %s", SDL_GetError())
#define b3_img_fatal(failed) b3_fatal(failed ": %s", IMG_GetError())

_Noreturn void b3_fatal_(
    const char *file,
    int line,
    const char *function,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "%s:%d(%s): ", file, line, function);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);

    exit(1);
}

static SDL_Window *window;
static SDL_Surface *sprite;

static void init(void) {
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
        b3_sdl_fatal("SDL_Init");
    atexit(SDL_Quit);
    if((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
        b3_img_fatal("IMG_Init");
    atexit(IMG_Quit);

    window = SDL_CreateWindow(
        "3omns",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        480,
        SDL_WINDOW_OPENGL
    );
    if(!window)
        b3_sdl_fatal("SDL_CreateWindow");
    SDL_DisableScreenSaver();

    sprite = IMG_Load("sprites.png");
    if(!sprite)
        b3_img_fatal("IMG_Load");
}

static void shutdown(void) {
    SDL_FreeSurface(sprite);
    SDL_DestroyWindow(window);
}

int main(void) {
    init();

    SDL_Surface *screen = SDL_GetWindowSurface(window);
    if(!screen)
        b3_sdl_fatal("SDL_GetWindowSurface");
    SDL_BlitSurface(sprite, NULL, screen, NULL);
    SDL_UpdateWindowSurface(window);

    SDL_Delay(3000);

    shutdown();
    return 0;
}
