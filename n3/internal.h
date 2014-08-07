/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    n3 - net communication library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

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

#ifndef __internal_h__
#define __internal_h__

#include "n3.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>


struct n3_buffer {
    int ref_count;
    n3_free free;
    size_t size;
    size_t cap;
    uint8_t buf[];
};

enum flags { // Must fit in 4 bits.
    ACK = 1 << 0,
    FIN = 1 << 1,
    // TODO: PING?
    // TODO: RESENT, maybe useful for informational/statistical purposes?

    ALL_FLAGS = ACK | FIN
};

typedef uint16_t sequence;
#define SEQUENCE_BITS 16

struct packet {
    n3_channel channel;
    sequence seq;
    n3_buffer *buffer;
    struct timespec time;
};

struct pool {
    struct packet *packets;
    int size;
    int count;
};

struct simplex_channel_state {
    sequence seq;
    struct pool pool;
};

struct duplex_channel_state {
    struct simplex_channel_state send;
    struct simplex_channel_state recv;
};

struct link_state {
    n3_host remote;

    struct duplex_channel_state ordered_states[
        N3_ORDERED_CHANNEL_MAX - N3_ORDERED_CHANNEL_MIN + 1
    ];
    struct simplex_channel_state unordered_states[ // Send-only.
        N3_UNORDERED_CHANNEL_MAX - N3_UNORDERED_CHANNEL_MIN + 1
    ];
};
#define LINK_STATE_INIT {{{0}}} // FIXME: this is ridiculous.

// TODO: combine code for link_states and pool.
struct link_states {
    struct link_state *states;
    int size;
    int count;
};
#define LINK_STATES_INIT {NULL, 0, 0}

#define FOR_EACH_LINK_STATE(link_state_var, link_states) for( \
    struct link_state *link_state_var = &(link_states)->states[0], \
            *end__ = &(link_states)->states[(link_states)->count]; \
    link_state_var < end__; \
    link_state_var++ \
)


void init_link_states(struct link_states *restrict link_states);
void destroy_link_states(struct link_states *restrict link_states);

struct link_state *search_link_state(
    const struct link_states *restrict link_states,
    const n3_host *restrict remote
);
struct link_state *insert_link_state(
    struct link_states *restrict link_states,
    const n3_host *restrict remote
);


void send_ack(
    int socket_fd,
    const struct packet *restrict packet,
    const n3_host *restrict remote
);
void send_fin(int socket_fd, const n3_host *restrict remote);

void send_buffer(
    int socket_fd,
    struct link_state *restrict link_state,
    n3_channel channel,
    n3_buffer *restrict buffer,
    _Bool reliable
);

size_t receive_packet(
    int socket_fd,
    enum flags *restrict flags,
    struct packet *restrict packet, // Only fills in the metadata, not buffer.
    void *restrict buf,
    size_t size,
    n3_host *restrict remote
);

void handle_ack_packet(
    struct link_state *restrict link_state,
    const struct packet *restrict ack
);


#endif
