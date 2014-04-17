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

    b3_entity_id id; // Sync'd with server?
    b3_pos pos; // Sync'd with server.
    int life; // Sync'd with server.

    b3_image *image;

    void *data;
    b3_free_entity_data_callback free_data_callback;
};

struct b3_entity_pool {
    int ref_count;

    b3_map *map;
    b3_size map_size;

    int size;
    int count;

    b3_entity_id id_generator;

    struct index_entry *index;

    int inactive_count;
    b3_entity **inactive;

    b3_entity entities[];
};

#define SIZEOF_ENTITY_POOL(pool, size) \
        (sizeof(*(pool)) + (size) * sizeof((pool)->entities[0]))

struct draw_entity_data {
    b3_size tile_size;
    b3_pos origin;
};


b3_entity_pool *b3_new_entity_pool(int size, b3_map *restrict map) {
    b3_entity_pool *pool = b3_malloc(SIZEOF_ENTITY_POOL(pool, size), 1);
    pool->map = b3_ref_map(map);
    pool->map_size = b3_get_map_size(map);
    pool->size = size;
    pool->index = b3_malloc(size * sizeof(*pool->index), 1);
    pool->inactive_count = size;
    pool->inactive = b3_malloc(size * sizeof(*pool->inactive), 1);
    for(int i = 0; i < size; i++)
        pool->inactive[i] = &pool->entities[i];
    return b3_ref_entity_pool(pool);
}

b3_entity_pool *b3_ref_entity_pool(b3_entity_pool *restrict pool) {
    pool->ref_count++;
    return pool;
}

static void free_entity_data(b3_entity *restrict entity) {
    if(entity->data && entity->free_data_callback)
        entity->free_data_callback(entity, entity->data);
    entity->data = NULL;
}

static b3_entity_pool *deactivate_entity(b3_entity *restrict entity) {
    if(!entity || !entity->id)
        return NULL;

    free_entity_data(entity);
    b3_free_image(entity->image);

    b3_entity_pool *pool = entity->pool;
    memset(entity, 0, sizeof(*entity));
    return pool;
}

void b3_free_entity_pool(b3_entity_pool *restrict pool) {
    if(pool && !--(pool->ref_count)) {
        for(int i = 0; i < pool->size; i++)
            deactivate_entity(&pool->entities[i]);
        b3_free(pool->index, 0);
        b3_free(pool->inactive, 0);
        b3_free_map(pool->map);
        b3_free(pool, SIZEOF_ENTITY_POOL(pool, pool->size));
    }
}

b3_entity *b3_claim_entity(
    b3_entity_pool *restrict pool,
    b3_free_entity_data_callback free_data_callback
) {
    if(!pool->inactive_count)
        b3_fatal("No available entities; need a bigger pool");

    b3_entity *entity = pool->inactive[--(pool->inactive_count)];
    entity->pool = pool;
    entity->id = ++(pool->id_generator);
    entity->free_data_callback = free_data_callback;
    pool->index[pool->count++] = (struct index_entry){entity->id, entity};
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
        sizeof(pool->index[0]),
        compare_index_entries
    );
}

void b3_release_entity(b3_entity *restrict entity) {
    b3_entity_id id = entity->id;
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
        (--(pool->count) - index) * sizeof(struct index_entry)
    );
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
    return entity;
}

int b3_get_entity_life(b3_entity *restrict entity) {
    return entity->life;
}

b3_entity *b3_set_entity_life(b3_entity *restrict entity, int life) {
    entity->life = life;
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
    const b3_entity *end = pool->entities + pool->size;
    for(b3_entity *e = pool->entities; e < end; e++) {
        if(e->id)
            callback(e, callback_data);
    }
}

static void draw_entity(b3_entity *restrict entity, void *callback_data) {
    const struct draw_entity_data *restrict draw_data = callback_data;

    if(entity->image) {
        b3_draw_image(entity->image, &B3_RECT(
            draw_data->origin.x + entity->pos.x * draw_data->tile_size.width,
            draw_data->origin.y + entity->pos.y * draw_data->tile_size.height,
            draw_data->tile_size.width,
            draw_data->tile_size.height
        ));
    }
}

void b3_draw_entities(
    b3_entity_pool *restrict pool,
    const b3_rect *restrict rect
) {
    b3_for_each_entity(pool, draw_entity, &(struct draw_entity_data){
        b3_get_map_tile_size(&pool->map_size, &rect->size),
        rect->pos,
    });
}
