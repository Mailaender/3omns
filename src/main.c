#include "b3/b3.h"
#include "l3/l3.h"

#include <stdlib.h>


int main(void) {
    b3_init("3omns", 640, 480);
    atexit(b3_quit);
    l3_init("game/init");
    atexit(l3_quit);

    while(!b3_process_events()) {
        b3_begin_scene();

        // TODO

        b3_end_scene();
        b3_sleep(10);
    }

    return 0;
}
