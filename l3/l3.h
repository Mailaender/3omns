#ifndef __l3_h__
#define __l3_h__

#include "b3/b3.h"


void l3_init(const char *restrict resource_path);
void l3_quit(void);


#define L3_TILE_COUNT 256 // Possible tile values: b3_tile is 8 bits.

extern b3_image *l3_tile_images[L3_TILE_COUNT];


b3_map *l3_generate_map(void);


#endif
