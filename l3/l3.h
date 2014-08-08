/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    l3 - Lua interface library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

    3omns is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    3omns is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with 3omns.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef l3_l3_h__
#define l3_l3_h__

#include "b3/b3.h"

#include <stddef.h>


void l3_init(
    const char *restrict resource_path,
    const char *restrict game,
    _Bool client,
    _Bool debug
);
void l3_quit(void);

void l3_enter_debugger(void);


#define L3_DUDE_COUNT 4

typedef struct l3_level l3_level;
struct l3_level {
    b3_map *map;
    b3_entity_pool *sprites;
    b3_entity_pool *entities;
    b3_entity_id dude_ids[L3_DUDE_COUNT];
};

#define L3_LEVEL_INIT {NULL, NULL, NULL, {0}}

static inline l3_level *l3_init_level(
    l3_level *restrict l,
    _Bool client,
    const b3_size *restrict map_size,
    int max_entities
) {
    int max_sprites = map_size->width * map_size->height * 2;

    *l = (l3_level)L3_LEVEL_INIT;
    l->map = b3_new_map(map_size);
    l->sprites = b3_new_entity_pool(1, max_sprites, l->map);
    l->entities = b3_new_entity_pool(!client, max_entities, l->map);
    return l;
}

static inline l3_level *l3_copy_level(
    l3_level *restrict new,
    l3_level *restrict old
) {
    new->map = b3_ref_map(old->map);
    new->sprites = b3_ref_entity_pool(old->sprites);
    new->entities = b3_ref_entity_pool(old->entities);
    for(int i = 0; i < L3_DUDE_COUNT; i++)
        new->dude_ids[i] = old->dude_ids[i];
    return new;
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
void l3_cull(l3_level *restrict level);
void l3_input(l3_level *restrict level, b3_input input);


char *l3_serialize_entity(b3_entity *restrict entity, size_t *restrict len);

void l3_set_sync_level(l3_level *restrict level);
l3_level l3_get_sync_level(void);

void l3_sync_entity(
    b3_entity_id id,
    const b3_pos *restrict pos,
    int life,
    const char *restrict serial,
    size_t serial_len
);
void l3_sync_deleted(b3_entity_id ids[], int count);


typedef struct l3_agent l3_agent;

l3_agent *l3_new_agent(l3_level *restrict level, int dude_index);
l3_agent *l3_ref_agent(l3_agent *restrict agent);
void l3_free_agent(l3_agent *restrict agent);

_Bool l3_think_agent(l3_agent *restrict agent, b3_ticks elapsed);


extern b3_image *l3_tile_images[B3_TILE_COUNT];
extern b3_image *l3_border_image;
extern b3_image *l3_heart_images[L3_DUDE_COUNT];


#endif
