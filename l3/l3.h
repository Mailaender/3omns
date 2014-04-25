#ifndef __l3_h__
#define __l3_h__

#include "b3/b3.h"

#include <stddef.h>


void l3_init(const char *restrict resource_path);
void l3_quit(void);


#define L3_DUDE_COUNT 4

typedef struct l3_level l3_level;
struct l3_level {
    b3_map *map;
    b3_entity_pool *sprites;
    b3_entity_pool *entities;
    b3_entity_id dude_ids[L3_DUDE_COUNT];
};

#define L3_LEVEL_INIT {NULL, NULL, NULL, {0}}

static inline l3_level l3_copy_level(l3_level *restrict l) {
    return (l3_level){
        b3_ref_map(l->map),
        b3_ref_entity_pool(l->sprites),
        b3_ref_entity_pool(l->entities),
        {l->dude_ids[0], l->dude_ids[1], l->dude_ids[2], l->dude_ids[3]},
    };
}

static inline void l3_free_level(l3_level *restrict l) {
    if(l) {
        b3_free_map(l->map);
        b3_free_entity_pool(l->sprites);
        b3_free_entity_pool(l->entities);
        *l = (l3_level)L3_LEVEL_INIT;
    }
}

l3_level l3_generate(void);

void l3_update(l3_level *restrict level, b3_ticks elapsed);
void l3_input(l3_level *restrict level, b3_input input);


extern b3_image *l3_tile_images[B3_TILE_COUNT];
extern b3_image *l3_border_image;
extern b3_image *l3_heart_images[L3_DUDE_COUNT];


#endif
