#include "b3/b3.h"
#include "l3/l3.h"

#include <stdlib.h>


#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define GAME_WIDTH WINDOW_HEIGHT
#define GAME_HEIGHT WINDOW_HEIGHT

#define HEART_SIZE 16


static const b3_size window_size = {WINDOW_WIDTH, WINDOW_HEIGHT};
static const b3_size game_size = {GAME_WIDTH, GAME_HEIGHT};
static const b3_rect game_rect = B3_RECT_INIT(0, 0, GAME_WIDTH, GAME_HEIGHT);

static _Bool handle_input(b3_input input, _Bool pressed, void *data) {
    l3_level *restrict level = data;

    if(input == B3_INPUT_BACK && pressed)
        return 1;
    // TODO: handle B3_INPUT_PAUSE.

    if(pressed)
        l3_input(level, input);

    return 0;
}

static void draw_border(
    const b3_size *restrict map_size,
    const b3_size *restrict tile_size
) {
    if(!l3_border_image)
        return;

    b3_rect rect = B3_RECT_INIT(
        game_size.width,
        0,
        tile_size->width,
        tile_size->height
    );
    for(int i = 0; i < map_size->height; i++) {
        b3_draw_image(l3_border_image, &rect);
        rect.y += tile_size->height;
    }
}

static void draw_hearts(
    l3_level *restrict level,
    const b3_size *restrict tile_size
) {
    int columns = L3_DUDE_COUNT * 2 - 1;
    int padding = (
        window_size.width
                - game_size.width
                - tile_size->width // For border.
                - columns * HEART_SIZE
    ) / 2;
    b3_rect rect = B3_RECT_INIT(
        game_size.width + tile_size->width + padding,
        0,
        HEART_SIZE,
        HEART_SIZE
    );

    for(int i = 0; i < L3_DUDE_COUNT; i++) {
        if(!l3_heart_images[i])
            continue;

        b3_entity *dude = b3_get_entity(level->entities, level->dude_ids[i]);
        if(!dude)
            continue;

        rect.y = window_size.height - padding - HEART_SIZE;

        int life = b3_get_entity_life(dude);
        for(int l = 0; l < life; l++) {
            b3_draw_image(l3_heart_images[i], &rect);
            rect.y -= HEART_SIZE;
        }
        rect.x += HEART_SIZE * 2;
    }
}

int main(void) {
    b3_init("3omns", &window_size);
    atexit(b3_quit);
    l3_init("res"); // TODO: installed path?
    atexit(l3_quit);

    l3_level level = l3_generate();
    b3_size map_size = b3_get_map_size(level.map);
    b3_size tile_size = b3_get_map_tile_size(&map_size, &game_size);

    b3_set_input_callback(handle_input, &level);

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
        l3_update(&level, 0 /* TODO */);

        b3_begin_scene();
        b3_draw_map(level.map, l3_tile_images, &game_rect);
        draw_border(&map_size, &tile_size);
        b3_draw_entities(level.entities, &game_rect);
        draw_hearts(&level, &tile_size);
        b3_end_scene();

        b3_sleep(10);
    }

    l3_free_level(&level);
    return 0;
}
