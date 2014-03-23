#include "b3/b3.h"
#include "l3/l3.h"

#include <stdlib.h>


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);
    l3_init();
    atexit(l3_quit);

    b3_image *sprites = b3_load_image("gfx/sprites.png");
    b3_image *blue = b3_new_sub_image(sprites, &(b3_rect){16, 16, 16, 16});
    b3_image *bomn = b3_new_sub_image(sprites, &(b3_rect){64, 0, 16, 16});
    b3_free_image(sprites);

    while(!b3_process_events()) {
        b3_begin_scene();
        b3_draw_image(blue, 100, 100);
        b3_draw_image(bomn, 200, 200);
        b3_end_scene();
        b3_sleep(10);
    }

    b3_free_image(blue);
    b3_free_image(bomn);
    return 0;
}
