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
#define b3_free free


void b3_init(const char *restrict title, int width, int height);
void b3_quit(void);


typedef struct b3_sprite b3_sprite;

b3_sprite *b3_load_sprite(const char *restrict filename);
b3_sprite *b3_sub_sprite(
    b3_sprite *restrict sprite,
    int x,
    int y,
    int width,
    int height
);
void b3_free_sprite(b3_sprite *restrict sprite);

void b3_begin_scene(void);
void b3_end_scene(void);

void b3_draw_sprite(b3_sprite *restrict sprite, int x, int y);


#endif
