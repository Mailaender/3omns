#include "b3.h"

#include <stdlib.h>


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);

    b3_image *images = b3_load_image("sprites.png");
    b3_image *image = b3_new_sub_image(images, 16, 16, 16, 16);
    b3_begin_scene();
    b3_draw_image(image, 100, 100);
    b3_end_scene();

    SDL_Delay(3000);

    b3_free_image(image);
    b3_free_image(images);
    return 0;
}
