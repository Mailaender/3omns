#ifndef __b3_h__
#define __b3_h__

#include <stddef.h>
#include <stdint.h>


#define b3_static_array_count(a) (sizeof(a)/sizeof(*(a)))


#define b3_fatal(...) \
        b3_fatal_(__FILE__, __LINE__, __func__, __VA_ARGS__)

_Noreturn void b3_fatal_(
    const char *restrict file,
    int line,
    const char *restrict function,
    const char *restrict format,
    ...
);

void *b3_malloc(size_t size, _Bool zero);
void *b3_alloc_copy(const void *restrict ptr, size_t size);
void *b3_realloc(void *restrict ptr, size_t size);
void b3_free(void *restrict ptr, size_t zero_size);


void b3_init(const char *restrict title, int width, int height);
void b3_quit(void);

_Bool b3_process_events(void);


typedef uint64_t b3_ticks;

b3_ticks b3_get_tick_count();
extern b3_ticks b3_tick_frequency;

static inline b3_ticks b3_secs_to_ticks(double secs) {
    return (b3_ticks)(secs * b3_tick_frequency + 0.5);
}

static inline double b3_ticks_to_secs(b3_ticks ticks) {
    return (double)ticks / b3_tick_frequency;
}

static inline double b3_get_duration(
    b3_ticks start_ticks,
    b3_ticks finish_ticks
) {
    return b3_ticks_to_secs(finish_ticks - start_ticks);
}


typedef struct b3_rect b3_rect;
struct b3_rect {
    int x;
    int y;
    int width;
    int height;
};

void b3_begin_scene(void);
void b3_end_scene(void);


typedef struct b3_image b3_image;

b3_image *b3_load_image(const char *restrict filename);
b3_image *b3_new_sub_image(
    b3_image *restrict image,
    const b3_rect *restrict rect
);
b3_image *b3_ref_image(b3_image *restrict image);
void b3_free_image(b3_image *restrict image);

void b3_draw_image(b3_image *restrict image, int x, int y);


typedef struct b3_sprite_frame b3_sprite_frame;
struct b3_sprite_frame {
    int image_index;
    double duration;
};

typedef struct b3_sprite_definition b3_sprite_definition;
struct b3_sprite_definition {
    int image_count;
    b3_rect *image_rects;

    int frame_count;
    b3_sprite_frame *frames;
};
#define B3_STATIC_SPRITE_DEFINITION(image_rects, frames) { \
    b3_static_array_count(image_rects), (image_rects), \
    b3_static_array_count(frames), (frames), \
}

typedef struct b3_sprite_type b3_sprite_type;
typedef struct b3_sprite b3_sprite;

b3_sprite_type *b3_new_sprite_type(
    b3_image *restrict image,
    const b3_sprite_definition *restrict definition
);
b3_sprite_type *b3_ref_sprite_type(b3_sprite_type *restrict sprite_type);
void b3_free_sprite_type(b3_sprite_type *restrict sprite_type);

b3_sprite *b3_new_sprite(
    b3_sprite_type *restrict type,
    int starting_frame_index
);
b3_sprite *b3_new_simple_sprite(
    b3_image *restrict image,
    const b3_rect *restrict sub_rect
);
b3_sprite *b3_ref_sprite(b3_sprite *restrict sprite);
void b3_free_sprite(b3_sprite *restrict sprite);

void b3_update_sprite(b3_sprite *restrict sprite, b3_ticks elapsed);
void b3_draw_sprite(b3_sprite *restrict sprite, int x, int y);


#endif
