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
char *b3_copy_string(const char *restrict string);
void *b3_realloc(void *restrict ptr, size_t size);
void b3_free(void *restrict ptr, size_t zero_size);


void b3_init(const char *restrict title, int width, int height);
void b3_quit(void);

extern int b3_width;
extern int b3_height;

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

void b3_sleep(int ms);


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

int b3_get_image_width(b3_image *restrict image);
int b3_get_image_height(b3_image *restrict image);

void b3_draw_image(b3_image *restrict image, const b3_rect *restrict rect);


typedef uint8_t b3_tile;

#define B3_TILE_COUNT 256

typedef struct b3_map b3_map;

b3_map *b3_new_map(int width, int height);
b3_map *b3_ref_map(b3_map *restrict map);
void b3_free_map(b3_map *restrict map);

int b3_get_map_width(b3_map *restrict map);
int b3_get_map_height(b3_map *restrict map);
b3_tile b3_get_map_tile(b3_map *restrict map, int x, int y);
b3_map *b3_set_map_tile(b3_map *restrict map, int x, int y, b3_tile tile);

void b3_draw_map(
    b3_map *restrict map,
    b3_image *tile_images[B3_TILE_COUNT],
    const b3_rect *restrict rect
);

static inline b3_rect b3_get_absolute_tile_rect(
    int map_width,
    int map_height,
    const b3_rect *restrict draw_map_rect,
    int tile_x,
    int tile_y
) {
    int tile_width = draw_map_rect->width / map_width;
    int tile_height = draw_map_rect->height / map_height;
    return (b3_rect){
        draw_map_rect->x + tile_x * tile_width,
        draw_map_rect->y + tile_y * tile_height,
        tile_width,
        tile_height,
    };
}


#endif
