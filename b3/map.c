/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    b3 - base library for 3omns
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

#include "b3.h"


struct b3_map {
    int ref_count;
    b3_size size;
    b3_tile tiles[];
};

#define SIZEOF_MAP(map, size) ( \
    sizeof(*(map)) + (size)->width * (size)->height * sizeof((map)->tiles[0]) \
)

#define MAP_TILE(map, pos) \
        ((map)->tiles[(pos)->x + (pos)->y * (map)->size.width])


b3_map *b3_new_map(const b3_size *restrict size) {
    b3_map *map = b3_malloc(SIZEOF_MAP(map, size), 1);
    map->size = *size;
    return b3_ref_map(map);
}

b3_map *b3_ref_map(b3_map *restrict map) {
    map->ref_count++;
    return map;
}

void b3_free_map(b3_map *restrict map) {
    if(map && !--map->ref_count)
        b3_free(map, SIZEOF_MAP(map, &map->size));
}

b3_size b3_get_map_size(b3_map *restrict map) {
    return map->size;
}

b3_tile b3_get_map_tile(b3_map *restrict map, const b3_pos *restrict pos) {
    return MAP_TILE(map, pos);
}

b3_map *b3_set_map_tile(
    b3_map *restrict map,
    const b3_pos *restrict pos,
    b3_tile tile
) {
    MAP_TILE(map, pos) = tile;
    return map;
}

void b3_draw_map(
    b3_map *restrict map,
    b3_image *tile_images[B3_TILE_COUNT],
    const b3_rect *restrict rect
) {
    b3_size tile_size = b3_get_map_tile_size(&map->size, &rect->size);

    b3_tile *tile = map->tiles;
    b3_rect tile_rect = {.pos = rect->pos, .size = tile_size};
    for(int y = 0; y < map->size.height; y++) {
        for(int x = 0; x < map->size.width; x++) {
            b3_image *image = tile_images[*tile++];
            if(image)
                b3_draw_image(image, &tile_rect);
            tile_rect.x += tile_size.width;
        }
        tile_rect.x = rect->x;
        tile_rect.y += tile_size.height;
    }
}
