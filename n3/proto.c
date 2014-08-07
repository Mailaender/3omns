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
#include <time.h>
#include <unistd.h> // For _POSIX_TIMERS.


#if(_POSIX_TIMERS <= 0)
#error no POSIX timers :(
#endif


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

struct link_state *init_link_state(
    struct link_state *restrict state,
    const n3_host *restrict remote
) {
    *state = (struct link_state)LINK_STATE_INIT;
    state->remote = *remote;
    return state;
}

void destroy_link_state(struct link_state *restrict state) {
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(state->ordered_states); i++)
        destroy_duplex_channel_state(&state->ordered_states[i]);
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(state->unordered_states); i++)
        destroy_simplex_channel_state(&state->unordered_states[i]);
    *state = (struct link_state)LINK_STATE_INIT;
}

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
        add_packet(&send_state->pool, &p, NULL);
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

    remove_packet(&send_state->pool, &ack->seq);
}