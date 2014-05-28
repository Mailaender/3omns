#ifndef __3omns_h__
#define __3omns_h__

#include "b3/b3.h"
#include "l3/l3.h"
#include "n3/n3.h"

#include <stddef.h>
#include <stdio.h>


#define DEFAULT_PORT 30325


struct args {
    _Bool debug;
    _Bool debug_network;
    _Bool serve;
    _Bool client;
    const char *hostname;
    n3_port port;
};
#define ARGS_INIT_DEFAULT {0, 0, 0, 0, NULL, DEFAULT_PORT}

extern struct args args;


#define DEBUG_FILE stdout
#define DEBUG_NETWORK_FILE stdout

#define DEBUG_PRINT(...) \
        ((void)(args.debug && fprintf(DEBUG_FILE, __VA_ARGS__)))


struct round {
    _Bool initialized;

    l3_level level;
    b3_size map_size;
    b3_size tile_size;

    l3_agent *agents[L3_DUDE_COUNT];

    _Bool paused;
};
#define ROUND_INIT {0, L3_LEVEL_INIT, {0,0}, {0,0}, {NULL}, 0}


void init_net(void);
void quit_net(void);

void notify_paused_changed(const struct round *restrict round);
void notify_updates(const struct round *restrict round);
void process_notifications(struct round *restrict round);


extern const b3_size window_size;
extern const b3_size game_size;
extern const b3_rect game_rect;


#endif
