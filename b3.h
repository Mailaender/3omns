#ifndef __b3_h__
#define __b3_h__

#include <stddef.h>
#include <stdint.h>


#define b3_static_array_count(a) (sizeof(a)/sizeof(*(a)))


#define b3_fatal(format, ...) \
        b3_fatal_(__FILE__, __LINE__, __func__, format, __VA_ARGS__)

_Noreturn void b3_fatal_(
    const char *restrict file,
    int line,
    const char *restrict function,
    const char *restrict format,
    ...
);

void *b3_malloc(size_t size);
void b3_free(void *restrict ptr, size_t zero_size);


void b3_init(const char *restrict title, int width, int height);
void b3_quit(void);


uint64_t b3_get_tick_count();
extern uint64_t b3_tick_frequency;

static inline uint64_t b3_secs_to_ticks(double secs) {
    return (uint64_t)(secs * b3_tick_frequency + 0.5);
}

static inline double b3_ticks_to_secs(uint64_t ticks) {
    return (double)ticks / b3_tick_frequency;
}

static inline double b3_get_duration(
    uint64_t start_ticks,
    uint64_t finish_ticks
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


#endif
