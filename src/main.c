#include "b3/b3.h"
#include "l3/l3.h"

#include <stdlib.h>


static void draw_border(b3_map *restrict map) {
    int map_height = b3_get_map_height(map);
    int tile_height = 480/map_height;
    b3_rect rect = {480, 0, tile_height, 480/b3_get_map_width(map)};
    for(int i = 0; i < map_height; i++) {
        b3_draw_image(l3_border_image, &rect);
        rect.y += tile_height;
    }
}

int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);
    l3_init("res"); // TODO: installed path?
    atexit(l3_quit);

    b3_map *map = l3_generate();

    /* TODO: more like:
     * loop:
     *     calculate elapsed time since last run
     *     update entities at a fixed interval (40Hz?) in a loop to catch up
     *     process events (or maybe do this first or last?)
     *     do one render (only if there's enough time?)
     *     maybe run the lua garbage collector a bit? (again, only if time)
     *     if we've had to catch up every frame for a full second-ish, abort
     *     pause for the remainder of the time till the next frame update
     */
    while(!b3_process_events()) {
        b3_begin_scene();
        b3_draw_map(map, l3_tile_images, &(b3_rect){0, 0, 480, 480});
        draw_border(map);
        b3_end_scene();
        b3_sleep(10);
    }

    b3_free_map(map);
    return 0;
}
