#include "b3.h"


struct b3_map {
    int ref_count;
    int width;
    int height;
    b3_tile tiles[];
};

#define SIZEOF_MAP(map, width, height) \
        (sizeof(*(map)) + (width) * (height) * sizeof((map)->tiles[0]))

#define MAP_TILE(map, x, y) ((map)->tiles[(x) + (y) * (map)->width])


b3_map *b3_new_map(int width, int height) {
    b3_map *map = b3_malloc(SIZEOF_MAP(map, width, height), 1);
    map->width = width;
    map->height = height;
    return b3_ref_map(map);
}

b3_map *b3_ref_map(b3_map *restrict map) {
    map->ref_count++;
    return map;
}

void b3_free_map(b3_map *restrict map) {
    if(map && !--(map->ref_count))
        b3_free(map, SIZEOF_MAP(map, map->width, map->height));
}

int b3_get_map_width(b3_map *restrict map) {
    return map->width;
}

int b3_get_map_height(b3_map *restrict map) {
    return map->height;
}

b3_tile b3_get_map_tile(b3_map *restrict map, int x, int y) {
    return MAP_TILE(map, x, y);
}

b3_map *b3_set_map_tile(b3_map *restrict map, int x, int y, b3_tile tile) {
    MAP_TILE(map, x, y) = tile;
    return map;
}

void b3_draw_map(
    b3_map *restrict map,
    const b3_rect *restrict rect,
    b3_image *tile_images[B3_TILE_COUNT]
) {
    // TODO
}
