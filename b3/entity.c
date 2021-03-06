/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <https://chazomaticus.github.io/3omns/>
    b3 - base library for 3omns
    Copyright 2014-2016 Charles Lindsay <chaz@chazomatic.us>

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

#include "b3.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>


struct index_entry {
    b3_entity_id id;
    b3_entity *entity;
};

struct b3_entity {
    b3_entity_pool *pool;

    b3_entity_id id; // Sync'd with server.
    b3_pos pos; // Sync'd with server.
    int life; // Sync'd with server.
    _Bool dirty; // Whether we need to sync.

    b3_image *image;
    int z_order;
    b3_entity *z_prev;
    b3_entity *z_next;

    void *data;
    b3_free_entity_data_callback free_data;
};

struct b3_entity_pool {
    int ref_count;

    b3_map *map;
    b3_size map_size;

    _Bool generate_ids;
    b3_entity_id id_generator;

    int size;
    int count;

    struct index_entry *index;

    int inactive_count;
    b3_entity **inactive;

    b3_entity *z_first;

    b3_entity_id *released_ids;
    int released_id_count;

    _Bool dirty; // Whether any entities are dirty or have been deleted.

    b3_entity entities[];
};

#define SIZEOF_ENTITY_POOL(pool, size) \
        (sizeof(*(pool)) + (size) * sizeof((pool)->entities[0]))


b3_entity_pool *b3_new_entity_pool(
    _Bool generate_ids,
    int size,
    b3_map *restrict map
) {
    b3_entity_pool *pool = b3_malloc(SIZEOF_ENTITY_POOL(pool, size), 1);
    pool->map = b3_ref_map(map);
    pool->map_size = b3_get_map_size(map);
    pool->generate_ids = generate_ids;
    pool->size = size;
    pool->index = b3_malloc(size * sizeof(*pool->index), 1);
    pool->inactive_count = size;
    pool->inactive = b3_malloc(size * sizeof(*pool->inactive), 1);
    for(int i = 0; i < size; i++)
        pool->inactive[i] = &pool->entities[i];
    pool->released_ids = b3_malloc(size * sizeof(*pool->released_ids), 1);
    return b3_ref_entity_pool(pool);
}

b3_entity_pool *b3_ref_entity_pool(b3_entity_pool *restrict pool) {
    pool->ref_count++;
    return pool;
}

static void free_entity_data(b3_entity *restrict entity) {
    if(entity->data && entity->free_data)
        entity->free_data(entity, entity->data);
    entity->data = NULL;
}

static void z_list_insert(
    b3_entity_pool *restrict pool,
    b3_entity *restrict entity
) {
    if(!pool->z_first) {
        pool->z_first = entity;
        return;
    }

    for(b3_entity *restrict e = pool->z_first; ; e = e->z_next) {
        if(entity->z_order <= e->z_order) {
            entity->z_prev = e->z_prev;
            entity->z_next = e;

            if(e->z_prev)
                e->z_prev->z_next = entity;
            else
                pool->z_first = entity;

            e->z_prev = entity;
            return;
        }
        else if(!e->z_next) {
            entity->z_prev = e;
            e->z_next = entity;
            return;
        }
    }
}

static void z_list_remove(
    b3_entity_pool *restrict pool,
    b3_entity *restrict entity
) {
    if(entity->z_next)
        entity->z_next->z_prev = entity->z_prev;

    if(entity->z_prev)
        entity->z_prev->z_next = entity->z_next;
    else
        pool->z_first = entity->z_next;

    entity->z_next = NULL;
    entity->z_prev = NULL;
}

static b3_entity_pool *deactivate_entity(b3_entity *restrict entity) {
    if(!entity || !entity->id)
        return NULL;

    free_entity_data(entity);
    b3_free_image(entity->image);

    b3_entity_pool *pool = entity->pool;

    z_list_remove(pool, entity);
    memset(entity, 0, sizeof(*entity));

    return pool;
}

void b3_free_entity_pool(b3_entity_pool *restrict pool) {
    if(pool && !--pool->ref_count) {
        for(int i = 0; i < pool->size; i++)
            deactivate_entity(&pool->entities[i]);
        b3_free_map(pool->map);
        b3_free(pool->index, 0);
        b3_free(pool->inactive, 0);
        b3_free(pool->released_ids, 0);
        b3_free(pool, SIZEOF_ENTITY_POOL(pool, pool->size));
    }
}

int b3_get_entity_pool_size(b3_entity_pool *restrict pool) {
    return pool->size;
}

_Bool b3_get_entity_pool_dirty(b3_entity_pool *restrict pool) {
    return pool->dirty;
}

b3_entity_pool *b3_set_entity_pool_dirty(
    b3_entity_pool *restrict pool,
    _Bool dirty
) {
    pool->dirty = dirty;
    return pool;
}

const b3_entity_id *b3_get_released_ids(
    b3_entity_pool *restrict pool,
    int *restrict count
) {
    *count = pool->released_id_count;
    return pool->released_ids;
}

void b3_clear_released_ids(b3_entity_pool *restrict pool) {
    pool->released_id_count = 0;
}

b3_entity *b3_claim_entity(
    b3_entity_pool *restrict pool,
    b3_entity_id id,
    b3_free_entity_data_callback free_data_callback
) {
    if(!pool->inactive_count)
        b3_fatal("No available entities; need a bigger pool");
    if((id && pool->generate_ids) || (!id && !pool->generate_ids))
        b3_fatal("Must only generate entity ids when expected");

    if(!id)
        id = ++pool->id_generator;

    b3_entity *entity = pool->inactive[--pool->inactive_count];
    entity->pool = pool;
    entity->id = id;
    entity->dirty = 1;
    pool->dirty = 1;
    entity->free_data = free_data_callback;

    // This odd-looking index insert is to optimize for the local case where
    // ids are always increasing.
    int i;
    for(i = pool->count - 1; i >= 0 && pool->index[i].id > id; i--)
        continue;
    i++;
    memmove(
        &pool->index[i + 1],
        &pool->index[i],
        (pool->count - i) * sizeof(*pool->index)
    );
    pool->index[i] = (struct index_entry){entity->id, entity};
    pool->count++;

    z_list_insert(pool, entity);
    return entity;
}

static int compare_index_entries(const void *a_, const void *b_) {
    const struct index_entry *restrict a = a_;
    const struct index_entry *restrict b = b_;
    return (a->id == b->id ? 0 : (a->id < b->id ? -1 : 1));
}

static struct index_entry *search_index(
    b3_entity_pool *restrict pool,
    b3_entity_id id
) {
    return bsearch(
        &(struct index_entry){id},
        pool->index,
        pool->count,
        sizeof(*pool->index),
        compare_index_entries
    );
}

void b3_release_entity(b3_entity *restrict entity) {
    b3_entity_id id = (entity ? entity->id : 0);
    b3_entity_pool *pool = deactivate_entity(entity);
    if(!pool)
        return;

    pool->inactive[pool->inactive_count++] = entity;
    // TODO: wait for a bunch of indices to be removed, then only reshuffle
    // the index at the end.
    struct index_entry *entry = search_index(pool, id);
    ptrdiff_t index = entry - pool->index;
    memmove(
        entry,
        entry + 1,
        (--pool->count - index) * sizeof(*entry)
    );
    if(pool->released_id_count < pool->size)
        pool->released_ids[pool->released_id_count++] = id;
    pool->dirty = 1;
}

b3_entity *b3_get_entity(b3_entity_pool *restrict pool, b3_entity_id id) {
    struct index_entry *entry = search_index(pool, id);
    return (entry ? entry->entity : NULL);
}

b3_entity_id b3_get_entity_id(b3_entity *restrict entity) {
    return entity->id;
}

b3_pos b3_get_entity_pos(b3_entity *restrict entity) {
    return entity->pos;
}

b3_entity *b3_set_entity_pos(
    b3_entity *restrict entity,
    const b3_pos *restrict pos
) {
    entity->pos = *pos;
    return b3_set_entity_dirty(entity, 1);
}

int b3_get_entity_life(b3_entity *restrict entity) {
    return entity->life;
}

b3_entity *b3_set_entity_life(b3_entity *restrict entity, int life) {
    entity->life = life;
    return b3_set_entity_dirty(entity, 1);
}

_Bool b3_get_entity_dirty(b3_entity *restrict entity) {
    return entity->dirty;
}

b3_entity *b3_set_entity_dirty(b3_entity *restrict entity, _Bool dirty) {
    entity->dirty = dirty;
    if(dirty)
        entity->pool->dirty = dirty;
    return entity;
}

b3_entity *b3_set_entity_image(
    b3_entity *restrict entity,
    b3_image *restrict image
) {
    if(image != entity->image) {
        b3_free_image(entity->image);
        entity->image = (image ? b3_ref_image(image) : NULL);
    }
    return entity;
}

int b3_get_entity_z_order(b3_entity *restrict entity) {
    return entity->z_order;
}

b3_entity *b3_set_entity_z_order(b3_entity *restrict entity, int z_order) {
    if(z_order != entity->z_order) {
        z_list_remove(entity->pool, entity);
        entity->z_order = z_order;
        // TODO: batch the insert across multiple entities, maybe when drawing.
        z_list_insert(entity->pool, entity);
    }
    return entity;
}

void *b3_get_entity_data(b3_entity *restrict entity) {
    return entity->data;
}

b3_entity *b3_set_entity_data(b3_entity *restrict entity, void *entity_data) {
    if(entity_data != entity->data) {
        free_entity_data(entity);
        entity->data = entity_data;
    }
    return entity;
}

void b3_for_each_entity(
    b3_entity_pool *restrict pool,
    b3_entity_callback callback,
    void *callback_data
) {
    // Iterate backwards to optimize for the initial case where all entities
    // have been added in reverse id order, so we're actually iterating over
    // them in id order.  This makes transferring the state over the network
    // quicker, and doesn't hurt anything else.
    b3_entity *first = pool->entities;
    for(b3_entity *e = first + pool->size - 1; e >= first; e--) {
        if(e->id)
            callback(e, callback_data);
    }
}

void b3_draw_entities(
    b3_entity_pool *restrict pool,
    const b3_rect *restrict rect
) {
    b3_size tile_size = b3_get_map_tile_size(&pool->map_size, &rect->size);

    for(b3_entity *e = pool->z_first; e; e = e->z_next) {
        if(e->image) {
            b3_draw_image(e->image, &B3_RECT(
                rect->pos.x + e->pos.x * tile_size.width,
                rect->pos.y + e->pos.y * tile_size.height,
                tile_size.width,
                tile_size.height
            ));
        }
    }
}
