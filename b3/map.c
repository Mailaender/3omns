#include "b3.h"


b3_map *b3_new_map(int width, int height) {
    b3_map *map = b3_malloc(B3_SIZEOF_MAP(map, width, height), 1);
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
        b3_free(map, B3_SIZEOF_MAP(map, map->width, map->height));
}
