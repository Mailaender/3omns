#include "b3.h"

#include <stdlib.h>


static b3_rect bomn_sprite_rects[] = {
    {64,  0, 16, 16},
    {64, 16, 16, 16},
    {64, 32, 16, 16},
    {64, 48, 16, 16},
    {64, 64, 16, 16},
    {64, 80, 16, 16},
};
static b3_sprite_frame bomn_sprite_frames[] = {
    {5, 0.25}, {-1, 0.25}, {0, 0.25}, {-1, 0.25},
    {4, 0.25}, {-1, 0.25}, {0, 0.25}, {-1, 0.25},
    {3, 0.25}, {-1, 0.25}, {0, 0.25}, {-1, 0.25},
    {2, 0.25}, {-1, 0.25}, {0, 0.25}, {-1, 0.25},
    {1, 0.25}, {-1, 0.25}, {0, 0.1}, {-1, 0.1}, {0, 0.1}, {-1, 0.1}, {0, 0.1},
};


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);

    b3_image *image = b3_load_image("sprites.png");
    b3_sprite *blue = b3_new_simple_sprite(image, &(b3_rect){16, 16, 16, 16});

    b3_sprite_type *bomn_sprite_type = b3_new_sprite_type(
        image,
        &(b3_sprite_definition)B3_STATIC_SPRITE_DEFINITION(
            bomn_sprite_rects,
            bomn_sprite_frames
        )
    );
    b3_free_image(image);

    b3_sprite *bomn = b3_new_sprite(bomn_sprite_type, 0);
    b3_free_sprite_type(bomn_sprite_type);

    b3_ticks start = b3_get_tick_count();
    b3_ticks now = start;
    b3_ticks last;
    do {
        last = now;
        now = b3_get_tick_count();
        b3_update_sprite(bomn, now - last);

        b3_begin_scene();
        b3_draw_sprite(blue, 100, 100);
        b3_draw_sprite(bomn, 200, 200);
        b3_end_scene();

    } while(b3_get_duration(start, now) < 10.0);

    b3_free_sprite(blue);
    b3_free_sprite(bomn);
    return 0;
}
