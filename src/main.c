#include "b3/b3.h"
#include "l3/l3.h"

#include <stdlib.h>


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);
    l3_init("."); // TODO: installed path?
    atexit(l3_quit);

    b3_map *map = l3_generate_map();

    while(!b3_process_events()) {
        b3_begin_scene();

        // TODO

        b3_end_scene();
        b3_sleep(10);
    }

    b3_free_map(map);
    return 0;
}
