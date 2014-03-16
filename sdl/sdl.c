#include "b3.h"

#include <stddef.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>


struct b3_image {
    int ref_count;
    SDL_Texture *texture;
    SDL_Rect rect;
    b3_image *parent;
};


uint64_t b3_tick_frequency = 0;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;


static void init_sdl(void) {
    if(SDL_Init(SDL_INIT_NOPARACHUTE | SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0)
        b3_fatal("Error initializing SDL: %s", SDL_GetError());
    atexit(SDL_Quit);

    if((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
        b3_fatal("Error initializing SDL_image: %s", IMG_GetError());
    atexit(IMG_Quit);

    b3_tick_frequency = SDL_GetPerformanceFrequency();
}

static void create_window(const char *restrict title, int width, int height) {
    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_OPENGL
    );
    if(!window)
        b3_fatal("Error creating the main window: %s", SDL_GetError());

    SDL_DisableScreenSaver();
}

static void destroy_window(void) {
    if(window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
}

void b3_init(const char *restrict title, int width, int height) {
    init_sdl();

    create_window(title, width, height);
    atexit(destroy_window);

    // From the docs (<http://wiki.libsdl.org/SDL_CreateRenderer>) it sounds
    // like this prefers hardware-accelerated renderers.  If we need more
    // control, we may want to try creating different types of renderers in a
    // loop until one succeeds or something.
    renderer = SDL_CreateRenderer(window, -1, 0);
    if(!renderer)
        b3_fatal("Error creating rendering context: %s", SDL_GetError());
}

void b3_quit(void) {
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
}

uint64_t b3_get_tick_count() {
    return SDL_GetPerformanceCounter();
}

void b3_begin_scene(void) {
    SDL_RenderClear(renderer);
}

void b3_end_scene(void) {
    SDL_RenderPresent(renderer);
}

b3_image *b3_load_image(const char *restrict filename) {
    SDL_Surface *surface = IMG_Load(filename);
    if(!surface)
        b3_fatal("Error loading image %s: %s", filename, IMG_GetError());

    int width = surface->w;
    int height = surface->h;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(!texture)
        b3_fatal("Error creating texture from %s: %s", filename, SDL_GetError());

    b3_image *image = b3_malloc(sizeof(*image), 1);
    image->texture = texture;
    image->rect = (SDL_Rect){0, 0, width, height};
    return b3_ref_image(image);
}

b3_image *b3_new_sub_image(
    b3_image *restrict image,
    const b3_rect *restrict rect
) {
    b3_image *sub_image = b3_malloc(sizeof(*sub_image), 1);
    sub_image->texture = image->texture;
    sub_image->rect = (SDL_Rect){rect->x, rect->y, rect->width, rect->height};
    sub_image->parent = b3_ref_image(image);
    return b3_ref_image(sub_image);
}

b3_image *b3_ref_image(b3_image *restrict image) {
    image->ref_count++;
    return image;
}

void b3_free_image(b3_image *restrict image) {
    if(image && !--(image->ref_count)) {
        if(!image->parent)
            SDL_DestroyTexture(image->texture);
        b3_free_image(image->parent);
        b3_free(image, sizeof(*image));
    }
}

void b3_draw_image(b3_image *restrict image, int x, int y) {
    SDL_RenderCopy(
        renderer,
        image->texture,
        &image->rect,
        &(SDL_Rect){x, y, image->rect.w, image->rect.h}
    );
}
