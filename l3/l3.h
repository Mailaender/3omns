#ifndef __l3_h__
#define __l3_h__

#include "b3/b3.h"


void l3_init(const char *restrict resource_path);
void l3_quit(void);


extern b3_image *l3_border_image;
extern b3_image *l3_tile_images[B3_TILE_COUNT];


typedef struct l3_level l3_level;
struct l3_level {
    b3_map *map;
    b3_entity_pool *entities;
};

static inline l3_level l3_copy_level(l3_level *restrict level) {
    return (l3_level){
        b3_ref_map(level->map),
        b3_ref_entity_pool(level->entities)
    };
}

static inline void l3_free_level(l3_level *restrict level) {
    if(level) {
        b3_free_map(level->map);
        b3_free_entity_pool(level->entities);
    }
}

l3_level l3_generate(void);

void l3_update(l3_level *restrict level, b3_ticks elapsed);


#endif
