/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    n3 - net communication library for 3omns
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

#ifndef n3_internal_h__
#define n3_internal_h__

#include "n3.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>


void log_(n3_verbosity level, _Bool newline, const char *restrict format, ...);
#define log_error(...) log_(N3_ERRORS, 1, __VA_ARGS__)
#define log_warning(...) log_(N3_WARNINGS, 1, __VA_ARGS__)
#define log_debug(...) log_(N3_DEBUG, 1, __VA_ARGS__)
// Like echo -n, no newline:
#define log_n_debug(...) log_(N3_DEBUG, 0, __VA_ARGS__)


#define PROTO_VERSION 1 // Must fit in 4 bits.


enum flags { // Must fit in 4 bits.
    PING = 1 << 0, // Also means "connect".
    ACK = 1 << 1,
    FIN = 1 << 2,
    // TODO: RESENT, maybe useful for informational/statistical purposes?

    ALL_FLAGS = PING | ACK | FIN
};


typedef uint16_t sequence;
#define SEQUENCE_BITS 16

static inline int compare_sequence(sequence a, sequence b) {
    if(a == b)
        return 0;

    // Allow for wrap-around.
    const sequence half = 1 << (SEQUENCE_BITS - 1);
    if((a < b && b - a < half) || (a > b && a - b > half))
        return -1;

    return 1;
}


struct packet {
    n3_channel channel; // Convenience; can be determined elsewhere.
    sequence seq;
    n3_buffer *buffer;
    struct timespec time;
};

static inline void destroy_packet(struct packet *restrict p) {
    n3_free_buffer(p->buffer);
}

static inline int compare_packet(const void *key_, const void *member_) {
    const sequence *restrict key = key_;
    const struct packet *restrict member = member_;
    return compare_sequence(*key, member->seq);
}


#define OL_NAME pool
#define OL_ITEM_TYPE struct packet
#define OL_ITEM_NAME packet
#define OL_KEY_TYPE sequence
#define OL_COMPARATOR compare_packet
#define OL_ITEM_DESTRUCTOR destroy_packet
#include "ordered_list.h" // struct pool


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

    struct timespec send_time; // Only tracks ACK-able sends.
    struct timespec recv_time;

    // TODO: allow the user to specify how many states they'll use, so we don't
    // waste space (and processing time when resending).
    struct duplex_channel_state ordered_states[
        N3_ORDERED_CHANNEL_MAX - N3_ORDERED_CHANNEL_MIN + 1
    ];
    struct simplex_channel_state unordered_states[ // Send-only.
        N3_UNORDERED_CHANNEL_MAX - N3_UNORDERED_CHANNEL_MIN + 1
    ];
};
#define LINK_STATE_INIT {{{0}}} // FIXME: this is ridiculous.

struct link_state *init_link_state(
    struct link_state *restrict state,
    const n3_host *restrict remote
);
void destroy_link_state(struct link_state *restrict state);

static inline int compare_link_state(const void *key_, const void *member_) {
    const n3_host *restrict key = key_;
    const struct link_state *restrict member = member_;
    return n3_compare_hosts(key, &member->remote);
}


#define OL_NAME link_states
#define OL_ITEM_TYPE struct link_state
#define OL_ITEM_NAME link_state
#define OL_ITEMS_NAME links
#define OL_KEY_TYPE n3_host
#define OL_COMPARATOR compare_link_state
#define OL_ITEM_DESTRUCTOR destroy_link_state
#include "ordered_list.h" // struct link_states

static inline struct link_state *insert_link_state(
    struct link_states *restrict links,
    const n3_host *restrict remote
) {
    struct link_state link;
    return add_link_state(links, init_link_state(&link, remote), remote);
}


struct n3_buffer {
    int ref_count;
    n3_free free;
    size_t size;
    size_t cap;
    uint8_t buf[];
};

struct n3_terminal {
    int ref_count;
    n3_terminal_options options;
    int socket_fd;
    n3_link_filter filter_new_link;
    struct link_states links;
};


struct timespec *get_time(struct timespec *restrict ts);

void send_ping(
    int socket_fd,
    struct link_state *restrict link,
    const struct timespec *restrict now
);
void send_fin(
    int socket_fd,
    struct link_state *restrict link,
    const struct timespec *restrict now
);

void send_buffer(
    int socket_fd,
    struct link_state *restrict link,
    n3_channel channel,
    n3_buffer *restrict buffer,
    _Bool reliable
);

n3_buffer *receive_buffer(
    n3_terminal *restrict terminal,
    n3_channel *restrict channel,
    void *new_link_filter_data,
    void *remote_unlink_callback_data,
    struct link_state **restrict link
);

void upkeep(
    n3_terminal *restrict terminal,
    void *remote_unlink_callback_data,
    const struct timespec *restrict now
);


#endif
