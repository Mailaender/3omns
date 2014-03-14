#include "b3.h"

#include <stddef.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

struct b3_sprite {
    int refs;
    SDL_Texture *texture;
    SDL_Rect rect;
    struct b3_sprite *parent;
};
#define B3_SPRITE_INIT {0, NULL, {0,0,0,0}, NULL}


static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;


static void init_sdl(void) {
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
        b3_fatal("Error initializing SDL: %s", SDL_GetError());
    atexit(SDL_Quit);
    if((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
        b3_fatal("Error initializing SDL_image: %s", IMG_GetError());
    atexit(IMG_Quit);
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

static b3_sprite *ref_sprite(b3_sprite *restrict sprite) {
    sprite->refs++;
    return sprite;
}

b3_sprite *b3_load_sprite(const char *restrict filename) {
    SDL_Surface *surface = IMG_Load(filename);
    if(!surface)
        b3_fatal("Error loading sprite %s: %s", filename, IMG_GetError());

    int width = surface->w;
    int height = surface->h;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(!texture)
        b3_fatal("Error creating texture from %s: %s", filename, SDL_GetError());

    b3_sprite *sprite = b3_malloc(sizeof(*sprite));
    *sprite = (b3_sprite)B3_SPRITE_INIT;
    sprite->texture = texture;
    sprite->rect = (SDL_Rect){0, 0, width, height};
    return ref_sprite(sprite);
}

b3_sprite *b3_sub_sprite(
    b3_sprite *restrict sprite,
    int x,
    int y,
    int width,
    int height
) {
    b3_sprite *sub_sprite = b3_malloc(sizeof(*sub_sprite));
    *sub_sprite = (b3_sprite)B3_SPRITE_INIT;
    sub_sprite->texture = sprite->texture;
    sub_sprite->rect = (SDL_Rect){x, y, width, height};
    sub_sprite->parent = ref_sprite(sprite);
    return ref_sprite(sub_sprite);
}

void b3_free_sprite(b3_sprite *restrict sprite) {
    if(sprite) {
        b3_sprite *parent = sprite->parent;
        if(!(--(sprite->refs))) {
            if(!parent)
                SDL_DestroyTexture(sprite->texture);
            b3_free(sprite);
        }
        b3_free_sprite(parent);
    }
}

void b3_begin_scene(void) {
    SDL_RenderClear(renderer);
}

void b3_end_scene(void) {
    SDL_RenderPresent(renderer);
}

void b3_draw_sprite(b3_sprite *restrict sprite, int x, int y) {
    SDL_RenderCopy(
        renderer,
        sprite->texture,
        &sprite->rect,
        &(SDL_Rect){x, y, sprite->rect.w, sprite->rect.h}
    );
}
