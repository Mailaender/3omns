#include "b3.h"

#include <stdlib.h>


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);

    b3_sprite *sprite = b3_load_sprite("sprites.png");
    b3_begin_scene();
    b3_draw_sprite(sprite);
    b3_end_scene();

    SDL_Delay(3000);

    return 0;
}
