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

static _Bool paused = 0;


static _Bool handle_input(b3_input input, _Bool pressed, void *data) {
    l3_level *restrict level = data;

    if(!pressed)
        return 0;

    switch(input) {
    case B3_INPUT_BACK:
        return 1;
    case B3_INPUT_PAUSE:
        paused = !paused;
        return 0;
    default:
        if(!paused)
            l3_input(level, input);
        return 0;
    }
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

static void loop(l3_level *restrict level) {
    const b3_ticks frame_ticks = b3_secs_to_ticks(0.015625); // 64 FPS.
    const b3_ticks draw_ticks = b3_secs_to_ticks(0.03125); // 32 FPS.

    b3_font *font = b3_load_font(16, "res/ttf/Vera.ttf", 0);
    b3_text *paused_text = b3_new_text(font, "P A U S E D");
    b3_set_text_color(paused_text, 0xffff0000);
    b3_size paused_text_size = b3_get_text_size(paused_text);
    b3_free_font(font);

    b3_size map_size = b3_get_map_size(level->map);
    b3_size tile_size = b3_get_map_tile_size(&map_size, &game_size);

    b3_ticks time = 0;
    b3_ticks ticks = b3_get_tick_count();
    b3_ticks last_ticks;
    b3_ticks next_draw_ticks = 0;
    do {
        last_ticks = ticks;
        ticks = b3_get_tick_count();
        b3_ticks elapsed = ticks - last_ticks;

        if(!paused) {
            for(time += elapsed; time >= frame_ticks; time -= frame_ticks)
                l3_update(level, frame_ticks);
        }

        if(ticks >= next_draw_ticks) {
            next_draw_ticks = ticks + draw_ticks;

            b3_begin_scene();
            b3_draw_map(level->map, l3_tile_images, &game_rect);
            draw_border(&map_size, &tile_size);
            b3_draw_entities(level->entities, &game_rect);
            draw_hearts(level, &tile_size);
            if(paused)
                b3_draw_text(paused_text, &(b3_rect){.size = paused_text_size});
            b3_end_scene();
        }
    } while(!b3_process_events());
}

int main(void) {
    b3_init("3omns", &window_size);
    atexit(b3_quit);
    l3_init("res"); // TODO: installed path?
    atexit(l3_quit);

    l3_level level = l3_generate();

    b3_set_input_callback(handle_input, &level);

    loop(&level);

    l3_free_level(&level);
    return 0;
}
