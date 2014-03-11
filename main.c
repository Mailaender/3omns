#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>


#define b3_fatal(format, ...) \
        b3_fatal_(__FILE__, __LINE__, __func__, format, __VA_ARGS__)
#define b3_sdl_fatal(failed) b3_fatal(failed ": %s", SDL_GetError())

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

int main(void) {
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
        b3_sdl_fatal("SDL_Init");
    atexit(SDL_Quit);

    SDL_Window *window = SDL_CreateWindow(
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

    SDL_Delay(3000);

    SDL_DestroyWindow(window);
    return 0;
}
