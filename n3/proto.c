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

#include "n3.h"
#include "internal.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h> // For _POSIX_TIMERS.


#if(_POSIX_TIMERS <= 0)
#error no POSIX timers :(
#endif

#define PROTO_VERSION 1 // Must fit in 4 bits.

#define INIT_LINK_STATES_SIZE 8
#define INIT_POOL_SIZE 8


static void get_time(struct timespec *restrict ts) {
    clockid_t clock =
#if(defined(CLOCK_MONOTONIC_RAW))
        CLOCK_MONOTONIC_RAW
#elif(defined(_POSIX_MONOTONIC_CLOCK))
        CLOCK_MONOTONIC
#else
        CLOCK_REALTIME // TODO: just error in this case?
#endif
    ;

    if(clock_gettime(clock, ts) != 0)
        b3_fatal("Error getting time: %s", strerror(errno));
}

static void destroy_packet(struct packet *restrict p) {
    n3_free_buffer(p->buffer);
}

static void destroy_pool(struct pool *restrict p) {
    for(int i = 0; i < p->count; i++)
        destroy_packet(&p->packets[i]);
    b3_free(p->packets, 0);
}

static int compare_sequence(sequence a, sequence b) {
    if(a == b)
        return 0;

    // Allow for wrap-around.
    const sequence half = 1 << (SEQUENCE_BITS - 1);
    if((a < b && b - a < half) || (a > b && a - b > half))
        return -1;

    return 1;
}

static int compare_packet(const void *key_, const void *member_) {
    const sequence *restrict key = key_;
    const struct packet *restrict member = member_;
    return compare_sequence(*key, member->seq);
}

static struct packet *search_packet(
    const struct pool *restrict pool,
    sequence seq
) {
    return bsearch(
        &seq,
        pool->packets,
        pool->count,
        sizeof(*pool->packets),
        compare_packet
    );
}

static void add_packet(
    struct pool *restrict pool,
    const struct packet *restrict packet
) {
    if(pool->count >= pool->size) {
        if(!pool->size)
            pool->size = INIT_POOL_SIZE;
        else
            pool->size *= 2;

        pool->packets = b3_realloc(
            pool->packets,
            pool->size * sizeof(*pool->packets)
        );
    }

    // TODO: alternate mode where it sorts based on seq instead of appending.
    pool->packets[pool->count++] = *packet;
}

static void remove_packet(struct pool *restrict pool, sequence seq) {
    struct packet *p = search_packet(pool, seq);
    if(!p)
        return;

    destroy_packet(p);

    ptrdiff_t index = p - pool->packets;
    memmove(p, p + 1, (--pool->count - index) * sizeof(*p));
}

static void destroy_simplex_channel_state(
    struct simplex_channel_state *restrict scs
) {
    destroy_pool(&scs->pool);
}

static void destroy_duplex_channel_state(
    struct duplex_channel_state *restrict dcs
) {
    destroy_simplex_channel_state(&dcs->send);
    destroy_simplex_channel_state(&dcs->recv);
}

static void init_link_state(
    struct link_state *restrict state,
    const n3_host *restrict remote
) {
    *state = (struct link_state)LINK_STATE_INIT;
    state->remote = *remote;
}

static void destroy_link_state(struct link_state *restrict state) {
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(state->ordered_states); i++)
        destroy_duplex_channel_state(&state->ordered_states[i]);
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(state->unordered_states); i++)
        destroy_simplex_channel_state(&state->unordered_states[i]);
    *state = (struct link_state)LINK_STATE_INIT;
}

void init_link_states(struct link_states *restrict link_states) {
    *link_states = (struct link_states)LINK_STATES_INIT;
    link_states->size = INIT_LINK_STATES_SIZE;
    link_states->states
            = b3_malloc(link_states->size * sizeof(*link_states->states), 1);
}

void destroy_link_states(struct link_states *restrict link_states) {
    FOR_EACH_LINK_STATE(state, link_states)
        destroy_link_state(state);
    b3_free(link_states->states, 0);
    *link_states = (struct link_states)LINK_STATES_INIT;
}

static int compare_link_state(const void *key_, const void *member_) {
    const n3_host *restrict key = key_;
    const struct link_state *restrict member = member_;
    return n3_compare_hosts(key, &member->remote);
}

struct link_state *search_link_state(
    const struct link_states *restrict link_states,
    const n3_host *restrict remote
) {
    return bsearch(
        remote,
        link_states->states,
        link_states->count,
        sizeof(*link_states->states),
        compare_link_state
    );
}

struct link_state *insert_link_state(
    struct link_states *restrict link_states,
    const n3_host *restrict remote
) {
    struct link_state state;
    init_link_state(&state, remote);

    if(link_states->count >= link_states->size) {
        link_states->size *= 2;
        link_states->states = b3_realloc(
            link_states->states,
            link_states->size * sizeof(*link_states->states)
        );
    }

    // TODO: a binary search instead of a scan.
    int i = 0;
    FOR_EACH_LINK_STATE(state, link_states) {
        if(compare_link_state(remote, state) < 0)
            break;
        i++;
    }

    memmove(
        &link_states->states[i + 1],
        &link_states->states[i],
        (link_states->count - i) * sizeof(*link_states->states)
    );

    link_states->states[i] = state;
    link_states->count++;

    return &link_states->states[i];
}

// TODO: remove link state.

static struct simplex_channel_state *get_send_state(
    struct link_state *restrict state,
    n3_channel channel
) {
    if(N3_IS_ORDERED(channel))
        return &state->ordered_states[channel - N3_ORDERED_CHANNEL_MIN].send;
    return &state->unordered_states[channel - N3_UNORDERED_CHANNEL_MIN];
}

static sequence next_send_sequence(
    struct simplex_channel_state *restrict send_state
) {
    sequence seq = ++send_state->seq;
    if(!seq) // Skip 0, which is used for unreliable sends.
        seq = ++send_state->seq;
    return seq;
}

static void fill_proto_header(
    uint8_t buf[N3_HEADER_SIZE],
    enum flags flags,
    n3_channel channel,
    sequence seq
) {
    buf[0] = (PROTO_VERSION << 4) | (flags & 0xf);
    buf[1] = channel;
    buf[2] = seq >> 8 & 0xff;
    buf[3] = seq & 0xff;
}

static _Bool read_proto_header(
    uint8_t buf[N3_HEADER_SIZE],
    enum flags *restrict flags,
    n3_channel *restrict channel,
    sequence *restrict seq
) {
    int version = buf[0] >> 4;
    *flags = buf[0] & 0xf;
    *channel = buf[1];
    *seq = (sequence)buf[2] << 8 | (sequence)buf[3];

    if(version != PROTO_VERSION) // TODO: backwards compatibility?
        return 0;
    if(*flags & ~ALL_FLAGS) // Sanity check for unknown flags.
        return 0;
    // TODO: issue a warning?
    // TODO: more checks?

    return 1;
}

static void send_packet(
    int socket_fd,
    enum flags flags,
    const struct packet *restrict packet,
    const n3_host *restrict remote
) {
    uint8_t header[N3_HEADER_SIZE];
    fill_proto_header(header, flags, packet->channel, packet->seq);

    const void *bufs[] = {header, NULL};
    size_t sizes[] = {sizeof(header), 0};
    if(packet->buffer) {
        bufs[1] = packet->buffer->buf;
        sizes[1] = packet->buffer->cap;
    }

    n3_raw_send(socket_fd, B3_STATIC_ARRAY_COUNT(bufs), bufs, sizes, remote);
}

void send_ack(
    int socket_fd,
    const struct packet *restrict packet,
    const n3_host *restrict remote
) {
    struct packet ack = {
        .channel = packet->channel,
        .seq = packet->seq,
        .buffer = NULL,
    };
    send_packet(socket_fd, ACK, &ack, remote);
}

void send_fin(int socket_fd, const n3_host *restrict remote) {
    struct packet fin = {
        .channel = 0,
        .seq = 0,
        .buffer = NULL,
    };
    send_packet(socket_fd, FIN, &fin, remote);
}

void send_buffer(
    int socket_fd,
    struct link_state *restrict link_state,
    n3_channel channel,
    n3_buffer *restrict buffer,
    _Bool reliable
) {
    struct simplex_channel_state *send_state
            = get_send_state(link_state, channel);

    struct packet p = {
        .channel = channel,
        .seq = (reliable ? next_send_sequence(send_state) : 0),
        .buffer = n3_ref_buffer(buffer),
    };

    send_packet(socket_fd, 0, &p, &link_state->remote);

    if(reliable) {
        get_time(&p.time);
        add_packet(&send_state->pool, &p);
    }
    else
        n3_free_buffer(p.buffer);
}

size_t receive_packet(
    int socket_fd,
    enum flags *restrict flags,
    struct packet *restrict packet, // Only fills in the metadata, not buffer.
    void *restrict buf,
    size_t size,
    n3_host *restrict remote
) {
    uint8_t header[N3_HEADER_SIZE];
    void *bufs[] = {header, buf};
    size_t sizes[] = {sizeof(header), size};

    for(
        size_t received;
        (received = n3_raw_receive(
            socket_fd,
            B3_STATIC_ARRAY_COUNT(bufs),
            bufs,
            sizes,
            remote
        )) > 0;
    ) {
        if(read_proto_header(header, flags, &packet->channel, &packet->seq))
            return sizes[1];
    }
    return 0;
}

void handle_ack_packet(
    struct link_state *restrict link_state,
    const struct packet *restrict ack
) {
    struct simplex_channel_state *send_state
            = get_send_state(link_state, ack->channel);

    remove_packet(&send_state->pool, ack->seq);
}
