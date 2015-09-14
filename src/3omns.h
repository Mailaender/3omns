/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

    3omns is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    3omns is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with 3omns.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef src_3omns_h__
#define src_3omns_h__

#include "b3/b3.h"
#include "l3/l3.h"
#include "n3/n3.h"

#include <stddef.h>
#include <stdio.h>


#define DEFAULT_GAME "base"
#define DEFAULT_PORT 30325

struct args {
    const char *resources;
    const char *game;
    _Bool debug;
    _Bool debug_network;
    _Bool serve;
    _Bool client;
    const char *hostname;
    n3_port port;
    n3_verbosity protocol_verbosity;
};
#define ARGS_INIT_DEFAULT \
        {NULL, DEFAULT_GAME, 0, 0, 0, 0, NULL, DEFAULT_PORT, N3_SILENT}

void parse_args(struct args *restrict args, int argc, char *argv[]);

extern struct args args;


#define DEBUG_FILE stdout
#define DEBUG_NETWORK_FILE stdout

#define DEBUG_PRINT(...) \
        ((void)(args.debug && fprintf(DEBUG_FILE, __VA_ARGS__)))

struct debug_stats {
    b3_ticks reset_time;
    int loop_count;
    int update_count;
    int think_count;
    int render_count;
    int skip_count;
    int sent_packets;
    int received_packets;
    b3_text *text[7];
    b3_rect text_rect[7];
};


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

void update_net(struct round *restrict round);

void notify_paused_changed(const struct round *restrict round);
void notify_input(const struct round *restrict round, b3_input input);
void notify_updates(const struct round *restrict round);
void process_notifications(struct round *restrict round);

void get_net_debug_stats(struct debug_stats *restrict debug_stats);


extern const b3_size window_size;
extern const b3_size game_size;
extern const b3_rect game_rect;
extern const b3_size heart_size;


#endif
