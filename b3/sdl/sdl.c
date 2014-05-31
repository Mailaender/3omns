#include "b3.h"

#include <stddef.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>


struct b3_image {
    int ref_count;
    SDL_Texture *texture;
    b3_rect rect;
    b3_image *parent;
};

struct b3_font {
    int ref_count;
    TTF_Font *font;
};

struct b3_text {
    int ref_count;
    char *string;
    SDL_Texture *texture;
    b3_size size;
    b3_color color;
};


b3_size b3_window_size = {0, 0};
b3_ticks b3_tick_frequency = 0;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static _Bool quit = 0;

static b3_input_callback handle_input = NULL;


void b3_init(
    const char *restrict window_title,
    const b3_size *restrict window_size,
    b3_input_callback input_callback
) {
    quit = 0;

    if(SDL_Init(SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO) != 0)
        b3_fatal("Error initializing SDL: %s", SDL_GetError());

    if((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
        b3_fatal("Error initializing SDL_image: %s", IMG_GetError());

    if(TTF_Init())
        b3_fatal("Error initializing SDL_ttf: %s", TTF_GetError());

    b3_tick_frequency = (b3_ticks)SDL_GetPerformanceFrequency();

    window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        window_size->width,
        window_size->height,
        SDL_WINDOW_OPENGL
    );
    if(!window)
        b3_fatal("Error creating the main window: %s", SDL_GetError());
    b3_window_size = *window_size;

    SDL_DisableScreenSaver();

    // From the docs (<http://wiki.libsdl.org/SDL_CreateRenderer>) it sounds
    // like this prefers hardware-accelerated renderers.  If we need more
    // control, we may want to try creating different types of renderers in a
    // loop until one succeeds or something.
    renderer = SDL_CreateRenderer(window, -1, 0);
    if(!renderer)
        b3_fatal("Error creating rendering context: %s", SDL_GetError());

    handle_input = input_callback;
}

void b3_quit(void) {
    handle_input = NULL;

    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if(window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

static int keysym_to_input(const SDL_Keysym *restrict keysym) {
    // TODO: configuration or any kind of custom mapping.

    switch(keysym->sym) {
    case SDLK_ESCAPE: return B3_INPUT_BACK;
    case SDLK_PAUSE: return B3_INPUT_PAUSE;
    default: break;
    }

    switch(keysym->scancode) {
    case SDL_SCANCODE_GRAVE: return B3_INPUT_DEBUG;
    case SDL_SCANCODE_UP: return B3_INPUT_UP_0;
    case SDL_SCANCODE_DOWN: return B3_INPUT_DOWN_0;
    case SDL_SCANCODE_LEFT: return B3_INPUT_LEFT_0;
    case SDL_SCANCODE_RIGHT: return B3_INPUT_RIGHT_0;
    case SDL_SCANCODE_RCTRL: return B3_INPUT_FIRE_0;
    case SDL_SCANCODE_W: return B3_INPUT_UP_1;
    case SDL_SCANCODE_S: return B3_INPUT_DOWN_1;
    case SDL_SCANCODE_A: return B3_INPUT_LEFT_1;
    case SDL_SCANCODE_D: return B3_INPUT_RIGHT_1;
    case SDL_SCANCODE_LCTRL: return B3_INPUT_FIRE_1;
    default: break;
    }

    // These may not be desirable, depending on the layout.  A quick glance at
    // the 'poedia list of keyboard layouts seems to indicate it's safe.
    switch(keysym->sym) {
    case SDLK_RETURN: return B3_INPUT_FIRE_0; // For menus.
    case SDLK_p: return B3_INPUT_PAUSE;
    default: break;
    }

    return -1;
}

static _Bool handle_key_event(
    const SDL_KeyboardEvent *restrict event,
    void *input_callback_data
) {
    // TODO: gamepads or joysticks.

    if(!handle_input || event->repeat)
        return 0;

    int input = keysym_to_input(&event->keysym);
    if(input < 0)
        return 0;

    return handle_input(
        (b3_input)input,
        event->state == SDL_PRESSED,
        input_callback_data
    );
}

_Bool b3_process_events(void *input_callback_data) {
    SDL_Event event;
    while(SDL_PollEvent(&event) && !quit) {
        if(event.type == SDL_QUIT)
            quit = 1;
        else if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            if(handle_key_event(&event.key, input_callback_data))
                quit = 1;
        }
    }

    return quit;
}

b3_ticks b3_get_tick_count(void) {
    return (b3_ticks)SDL_GetPerformanceCounter();
}

void b3_sleep(int ms) {
    SDL_Delay((Uint32)ms);
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

    b3_size size = {surface->w, surface->h};

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(!texture)
        b3_fatal("Error creating texture from %s: %s", filename, SDL_GetError());

    b3_image *image = b3_malloc(sizeof(*image), 1);
    image->texture = texture;
    image->rect = (b3_rect){.size = size};
    return b3_ref_image(image);
}

b3_image *b3_new_sub_image(
    b3_image *restrict image,
    const b3_rect *restrict rect
) {
    b3_image *sub_image = b3_malloc(sizeof(*sub_image), 1);
    sub_image->texture = image->texture;
    sub_image->rect = *rect;
    sub_image->parent = b3_ref_image(image);
    return b3_ref_image(sub_image);
}

b3_image *b3_ref_image(b3_image *restrict image) {
    image->ref_count++;
    return image;
}

void b3_free_image(b3_image *restrict image) {
    if(image && !--image->ref_count) {
        if(!image->parent)
            SDL_DestroyTexture(image->texture);
        b3_free_image(image->parent);
        b3_free(image, sizeof(*image));
    }
}

b3_size b3_get_image_size(b3_image *restrict image) {
    return image->rect.size;
}

void b3_draw_image(b3_image *restrict image, const b3_rect *restrict rect) {
    const b3_rect *restrict r = &image->rect;
    SDL_RenderCopy(
        renderer,
        image->texture,
        &(SDL_Rect){r->x, r->y, r->width, r->height},
        &(SDL_Rect){rect->x, rect->y, rect->width, rect->height}
    );
}

b3_font *b3_load_font(int size, const char *restrict filename, int index) {
    TTF_Font *ttf = TTF_OpenFontIndex(filename, size, (int)index);
    if(!ttf)
        b3_fatal("Error loading font %s: %s", filename, TTF_GetError());

    b3_font *font = b3_malloc(sizeof(*font), 1);
    font->font = ttf;
    return b3_ref_font(font);
}

b3_font *b3_ref_font(b3_font *restrict font) {
    font->ref_count++;
    return font;
}

void b3_free_font(b3_font *restrict font) {
    if(font && !--font->ref_count) {
        TTF_CloseFont(font->font);
        b3_free(font, sizeof(*font));
    }
}

b3_text *b3_new_text(
    b3_font *restrict font,
    const char *restrict format,
    ...
) {
    va_list args;
    va_start(args, format);

    char *string = b3_copy_vformat(format, args);

    va_end(args);

    SDL_Surface *surface = TTF_RenderUTF8_Blended(
        font->font,
        string,
        (SDL_Color){255, 255, 255, 255}
    );
    if(!surface)
        b3_fatal("Error rendering text '%s': %s", string, TTF_GetError());

    b3_size size = {surface->w, surface->h};

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(!texture)
        b3_fatal("Error creating texture from string: %s", SDL_GetError());

    b3_text *text = b3_malloc(sizeof(*text), 1);
    text->string = string;
    text->texture = texture;
    text->size = size;
    text->color = 0xffffffff;
    return b3_ref_text(text);
}

b3_text *b3_ref_text(b3_text *restrict text) {
    text->ref_count++;
    return text;
}

void b3_free_text(b3_text *restrict text) {
    if(text && !--text->ref_count) {
        b3_free(text->string, 0);
        SDL_DestroyTexture(text->texture);
        b3_free(text, sizeof(*text));
    }
}

const char *b3_get_text_string(b3_text *restrict text) {
    return text->string;
}

b3_size b3_get_text_size(b3_text *restrict text) {
    return text->size;
}

b3_color b3_get_text_color(b3_text *restrict text) {
    return text->color;
}

b3_text *b3_set_text_color(b3_text *restrict text, b3_color color) {
    text->color = color;

    SDL_SetTextureColorMod(
        text->texture,
        (Uint8)B3_RED(color),
        (Uint8)B3_GREEN(color),
        (Uint8)B3_BLUE(color)
    );
    SDL_SetTextureAlphaMod(text->texture, (Uint8)B3_ALPHA(color));

    return text;
}

void b3_draw_text(b3_text *restrict text, const b3_rect *restrict rect) {
    SDL_RenderCopy(
        renderer,
        text->texture,
        NULL,
        &(SDL_Rect){rect->x, rect->y, rect->width, rect->height}
    );
}
