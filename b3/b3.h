/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
    b3 - base library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
*/

#ifndef __b3_h__
#define __b3_h__

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>


#define B3_STATIC_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define B3_STRINGIFY(x) B3_STRINGIFY_(x)
#define B3_STRINGIFY_(x) #x


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
char *b3_copy_vformat(const char *restrict format, va_list args);
char *b3_copy_format(const char *restrict format, ...);
void *b3_realloc(void *restrict ptr, size_t size);
void b3_free(void *restrict ptr, size_t zero_size);


typedef struct b3_pos b3_pos;
struct b3_pos {
    int x;
    int y;
};

typedef struct b3_size b3_size;
struct b3_size {
    int width;
    int height;
};

typedef struct b3_rect b3_rect;
struct b3_rect {
    union {
        b3_pos pos;
        struct {
            int x;
            int y;
        };
    };
    union {
        b3_size size;
        struct {
            int width;
            int height;
        };
    };
};

#define B3_RECT_INIT(x_, y_, w, h) {.x=(x_), .y=(y_), .width=(w), .height=(h)}
#define B3_RECT(x, y, w, h) ((b3_rect)B3_RECT_INIT((x), (y), (w), (h)))


typedef enum b3_input b3_input;
enum b3_input {
    B3_INPUT_BACK,
    B3_INPUT_PAUSE,
    B3_INPUT_DEBUG,
    B3_INPUT_UP_0,
    B3_INPUT_UP_1,
    B3_INPUT_UP_2,
    B3_INPUT_UP_3,
    B3_INPUT_DOWN_0,
    B3_INPUT_DOWN_1,
    B3_INPUT_DOWN_2,
    B3_INPUT_DOWN_3,
    B3_INPUT_LEFT_0,
    B3_INPUT_LEFT_1,
    B3_INPUT_LEFT_2,
    B3_INPUT_LEFT_3,
    B3_INPUT_RIGHT_0,
    B3_INPUT_RIGHT_1,
    B3_INPUT_RIGHT_2,
    B3_INPUT_RIGHT_3,
    B3_INPUT_FIRE_0,
    B3_INPUT_FIRE_1,
    B3_INPUT_FIRE_2,
    B3_INPUT_FIRE_3,
};

#define B3_INPUT_UP(x) (B3_INPUT_UP_0 + (x))
#define B3_INPUT_DOWN(x) (B3_INPUT_DOWN_0 + (x))
#define B3_INPUT_LEFT(x) (B3_INPUT_LEFT_0 + (x))
#define B3_INPUT_RIGHT(x) (B3_INPUT_RIGHT_0 + (x))
#define B3_INPUT_FIRE(x) (B3_INPUT_FIRE_0 + (x))
#define B3_INPUT_IS_PLAYER(i) ((i) >= B3_INPUT_UP_0)
#define B3_INPUT_PLAYER(i) (((i) - B3_INPUT_UP_0) % 4)

typedef _Bool (*b3_input_callback)(b3_input input, _Bool pressed, void *data);


void b3_init(
    const char *restrict window_title,
    const b3_size *restrict window_size,
    b3_input_callback input_callback
);
void b3_quit(void);

extern b3_size b3_window_size;

_Bool b3_process_events(void *input_callback_data);


typedef int64_t b3_ticks;

b3_ticks b3_get_tick_count(void);
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


void b3_begin_scene(void);
void b3_end_scene(void);


typedef uint32_t b3_color; // In ARGB order.

#define B3_COLOR(a, r, g, b) ( \
    ((uint32_t)(a) << 24 & 0xff000000) \
            | ((uint32_t)(r) << 16 & 0xff0000) \
            | ((uint32_t)(g) << 8 & 0xff00) \
            | ((uint32_t)(b) & 0xff) \
)

#define B3_ALPHA(c) ((int)((c) >> 24 & 0xff))
#define B3_RED(c) ((int)((c) >> 16 & 0xff))
#define B3_GREEN(c) ((int)((c) >> 8 & 0xff))
#define B3_BLUE(c) ((int)((c) & 0xff))


typedef struct b3_image b3_image;

b3_image *b3_load_image(const char *restrict filename);
b3_image *b3_new_sub_image(
    b3_image *restrict image,
    const b3_rect *restrict rect
);
b3_image *b3_ref_image(b3_image *restrict image);
void b3_free_image(b3_image *restrict image);

b3_size b3_get_image_size(b3_image *restrict image);

void b3_draw_image(b3_image *restrict image, const b3_rect *restrict rect);


typedef struct b3_font b3_font;

b3_font *b3_load_font(int size, const char *restrict filename, int index);
b3_font *b3_ref_font(b3_font *restrict font);
void b3_free_font(b3_font *restrict font);

typedef struct b3_text b3_text;

b3_text *b3_new_text(b3_font *restrict font, const char *restrict format, ...);
b3_text *b3_ref_text(b3_text *restrict text);
void b3_free_text(b3_text *restrict text);

const char *b3_get_text_string(b3_text *restrict text);
b3_size b3_get_text_size(b3_text *restrict text);
b3_color b3_get_text_color(b3_text *restrict text);
b3_text *b3_set_text_color(b3_text *restrict text, b3_color color);

void b3_draw_text(b3_text *restrict text, const b3_rect *restrict rect);


typedef uint8_t b3_tile;

#define B3_TILE_COUNT 256

typedef struct b3_map b3_map;

b3_map *b3_new_map(const b3_size *restrict size);
b3_map *b3_ref_map(b3_map *restrict map);
void b3_free_map(b3_map *restrict map);

b3_size b3_get_map_size(b3_map *restrict map);
b3_tile b3_get_map_tile(b3_map *restrict map, const b3_pos *restrict pos);
b3_map *b3_set_map_tile(
    b3_map *restrict map,
    const b3_pos *restrict pos,
    b3_tile tile
);

void b3_draw_map(
    b3_map *restrict map,
    b3_image *tile_images[B3_TILE_COUNT],
    const b3_rect *restrict rect
);

static inline b3_size b3_get_map_tile_size(
    const b3_size *restrict map_size,
    const b3_size *restrict draw_size
) {
    return (b3_size){
        draw_size->width / map_size->width,
        draw_size->height / map_size->height
    };
}


typedef struct b3_entity_pool b3_entity_pool;

typedef unsigned int b3_entity_id;
typedef struct b3_entity b3_entity;

typedef void (*b3_free_entity_data_callback)(
    b3_entity *entity,
    void *entity_data
);

typedef void (*b3_entity_callback)(b3_entity *entity, void *callback_data);

b3_entity_pool *b3_new_entity_pool(
    _Bool generate_ids,
    int size,
    b3_map *restrict map
);
b3_entity_pool *b3_ref_entity_pool(b3_entity_pool *restrict pool);
void b3_free_entity_pool(b3_entity_pool *restrict pool);

int b3_get_entity_pool_size(b3_entity_pool *restrict pool);
_Bool b3_get_entity_pool_dirty(b3_entity_pool *restrict pool);
b3_entity_pool *b3_set_entity_pool_dirty(
    b3_entity_pool *restrict pool,
    _Bool dirty
);

const b3_entity_id *b3_get_released_ids(
    b3_entity_pool *restrict pool,
    int *restrict count
);
void b3_clear_released_ids(b3_entity_pool *restrict pool);

b3_entity *b3_claim_entity(
    b3_entity_pool *restrict pool,
    b3_entity_id id,
    b3_free_entity_data_callback free_data_callback
);
void b3_release_entity(b3_entity *restrict entity);
b3_entity *b3_get_entity(b3_entity_pool *restrict pool, b3_entity_id id);

b3_entity_id b3_get_entity_id(b3_entity *restrict entity);
b3_pos b3_get_entity_pos(b3_entity *restrict entity);
b3_entity *b3_set_entity_pos(
    b3_entity *restrict entity,
    const b3_pos *restrict pos
);
int b3_get_entity_life(b3_entity *restrict entity);
b3_entity *b3_set_entity_life(b3_entity *restrict entity, int life);
_Bool b3_get_entity_dirty(b3_entity *restrict entity);
b3_entity *b3_set_entity_dirty(b3_entity *restrict entity, _Bool dirty);
b3_entity *b3_set_entity_image(
    b3_entity *restrict entity,
    b3_image *restrict image
);
int b3_get_entity_z_order(b3_entity *restrict entity);
b3_entity *b3_set_entity_z_order(b3_entity *restrict entity, int z_order);
void *b3_get_entity_data(b3_entity *restrict entity);
b3_entity *b3_set_entity_data(b3_entity *restrict entity, void *entity_data);

void b3_for_each_entity(
    b3_entity_pool *restrict pool,
    b3_entity_callback callback,
    void *callback_data
);

void b3_draw_entities(
    b3_entity_pool *restrict pool,
    const b3_rect *restrict rect
);


#endif
