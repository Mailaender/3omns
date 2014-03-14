#include "b3.h"

#include <stdlib.h>


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);

    b3_sprite *sprites = b3_load_sprite("sprites.png");
    b3_sprite *sprite = b3_sub_sprite(sprites, 16, 16, 16, 16);
    b3_begin_scene();
    b3_draw_sprite(sprite, 100, 100);
    b3_end_scene();

    SDL_Delay(3000);

    b3_free_sprite(sprite);
    b3_free_sprite(sprites);
    return 0;
}
