#ifndef __b3_h__
#define __b3_h__

#include <stddef.h>


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


typedef struct b3_rect {
    int x;
    int y;
    int width;
    int height;
} b3_rect;
#define B3_RECT_INIT {0, 0, 0, 0}


typedef struct b3_image b3_image;

b3_image *b3_load_image(const char *restrict filename);
b3_image *b3_new_sub_image(
    b3_image *restrict image,
    const b3_rect *restrict rect
);
void b3_free_image(b3_image *restrict image);

void b3_begin_scene(void);
void b3_end_scene(void);

void b3_draw_image(b3_image *restrict image, int x, int y);


#endif
