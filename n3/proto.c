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

#include "b3/b3.h"
#include "internal.h"
#include "n3.h"

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // For _POSIX_TIMERS.


#if(_POSIX_TIMERS <= 0)
#error no POSIX timers :(
#endif


struct timespec *get_time(struct timespec *restrict ts) {
    clockid_t clock =
#if(defined(CLOCK_MONOTONIC_RAW))
        CLOCK_MONOTONIC_RAW
#elif(defined(_POSIX_MONOTONIC_CLOCK))
        CLOCK_MONOTONIC
#else
        CLOCK_REALTIME // TODO: just error in this case?
#endif
    ;

    // TODO: turn this into a log_error call.
    if(clock_gettime(clock, ts) != 0)
        b3_fatal("Error getting time: %s", strerror(errno));
    return ts;
}

static void add_time_ms(struct timespec *restrict ts, long ms) {
    const long ns_per_ms = 1000000;
    const long ns_per_s = 1000000000;

    ts->tv_nsec += ms * ns_per_ms;
    while(ts->tv_nsec >= ns_per_s) {
        ts->tv_sec++;
        ts->tv_nsec -= ns_per_s;
    }
}

static int compare_timespec(
    const struct timespec *restrict t1,
    const struct timespec *restrict t2
) {
    if(t1->tv_sec != t2->tv_sec)
        return (t1->tv_sec > t2->tv_sec ? 1 : -1);
    // Seconds equal:
    if(t1->tv_nsec != t2->tv_nsec)
        return (t1->tv_nsec > t2->tv_nsec ? 1 : -1);
    return 0;
}

// Whether t1 + elapsed_ms <= t2.
static _Bool timeout_elapsed(
    const struct timespec *restrict t1,
    int elapsed_ms,
    const struct timespec *restrict t2
) {
    // TODO: instead of doing this addition just to check, store the resend
    // time directly.
    struct timespec timeout = *t1;
    add_time_ms(&timeout, elapsed_ms);
    return (compare_timespec(&timeout, t2) <= 0);
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
    struct link_state *restrict link,
    const n3_host *restrict remote
) {
    struct timespec now;
    get_time(&now);

    *link = (struct link_state)LINK_STATE_INIT;
    link->remote = *remote;
    link->send_time = now;
    link->recv_time = now;
    return link;
}

void destroy_link_state(struct link_state *restrict link) {
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(link->ordered_states); i++)
        destroy_duplex_channel_state(&link->ordered_states[i]);
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(link->unordered_states); i++)
        destroy_simplex_channel_state(&link->unordered_states[i]);
    *link = (struct link_state)LINK_STATE_INIT;
}

static struct simplex_channel_state *get_send_state(
    struct link_state *restrict link,
    n3_channel channel
) {
    if(N3_IS_ORDERED(channel))
        return &link->ordered_states[channel - N3_ORDERED_CHANNEL_MIN].send;
    return &link->unordered_states[channel - N3_UNORDERED_CHANNEL_MIN];
}

static sequence next_send_sequence(
    struct simplex_channel_state *restrict send_state
) {
    sequence seq = ++send_state->seq;
    if(!seq) // Skip 0, which is used for unreliable sends.
        seq = ++send_state->seq;
    return seq;
}

static sequence next_recv_sequence(sequence last) {
    sequence seq = last;
    seq++;
    if(!seq)
        seq++;
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
    size_t received_size,
    enum flags *restrict flags,
    n3_channel *restrict channel,
    sequence *restrict seq
) {
    if(received_size < N3_HEADER_SIZE) {
        log_warning("Incomplete packet, size %'zu; ignoring", received_size);
        return 0;
    }

    int version = buf[0] >> 4;
    *flags = buf[0] & 0xf;
    *channel = buf[1];
    *seq = (sequence)buf[2] << 8 | (sequence)buf[3];

    // TODO: backwards compatibility?
    if(version != PROTO_VERSION) {
        log_warning("Invalid packet version %d; ignoring", version);
        return 0;
    }
    // Sanity check for unknown flags.
    if(*flags & ~ALL_FLAGS) {
        log_warning("Invalid packet flags 0x%x; ignoring", *flags);
        return 0;
    }
    // TODO: more checks?

    return 1;
}

static void log_send_packet(
    enum flags flags,
    const struct packet *restrict packet,
    const n3_host *restrict remote
) {
    char address[N3_ADDRESS_SIZE] = {""};
    n3_get_host_address(remote, address, sizeof(address));
    n3_port port = n3_get_host_port(remote);

    log_n_debug("Sent to %s|%"PRIu16": ", address, port);

    if(flags & PING) {
        if(flags & ACK)
            log_debug("PONG");
        else
            log_debug("PING");
    }
    else if(flags & ACK)
        log_debug("ACK %"PRIu8"-%"PRIu16, packet->channel, packet->seq);
    else if(flags & FIN)
        log_debug("FIN");
    else
        log_debug("message %"PRIu8"-%"PRIu16, packet->channel, packet->seq);
}

static void send_packet(
    int socket_fd,
    struct link_state *restrict link,
    enum flags flags,
    struct packet *restrict packet,
    const struct timespec *restrict now
) {
    uint8_t header[N3_HEADER_SIZE];
    fill_proto_header(header, flags, packet->channel, packet->seq);

    const void *bufs[] = {header, NULL};
    size_t sizes[] = {sizeof(header), 0};
    if(packet->buffer) {
        bufs[1] = packet->buffer->buf;
        sizes[1] = packet->buffer->cap;
    }

    n3_raw_send(
        socket_fd,
        B3_STATIC_ARRAY_COUNT(bufs),
        bufs,
        sizes,
        &link->remote
    );

    packet->time = *now;
    if(flags == 0 || flags == PING)
        link->send_time = *now;

    log_send_packet(flags, packet, &link->remote);
}

void send_ping(
    int socket_fd,
    struct link_state *restrict link,
    const struct timespec *restrict now
) {
    struct packet ping = {
        .channel = 0,
        .seq = 0,
        .buffer = NULL,
    };
    send_packet(socket_fd, link, PING, &ping, now);
    destroy_packet(&ping);
}

static void send_pong(
    int socket_fd,
    struct link_state *restrict link,
    const struct timespec *restrict now
) {
    struct packet pong = {
        .channel = 0,
        .seq = 0,
        .buffer = NULL,
    };
    send_packet(socket_fd, link, PING | ACK, &pong, now);
    destroy_packet(&pong);
}

static void send_ack(
    int socket_fd,
    struct link_state *restrict link,
    const struct packet *restrict packet,
    const struct timespec *restrict now
) {
    struct packet ack = {
        .channel = packet->channel,
        .seq = packet->seq,
        .buffer = NULL,
    };
    send_packet(socket_fd, link, ACK, &ack, now);
    destroy_packet(&ack);
}

void send_fin(
    int socket_fd,
    struct link_state *restrict link,
    const struct timespec *restrict now
) {
    struct packet fin = {
        .channel = 0,
        .seq = 0,
        .buffer = NULL,
    };
    send_packet(socket_fd, link, FIN, &fin, now);
    destroy_packet(&fin);
}

void send_buffer(
    int socket_fd,
    struct link_state *restrict link,
    n3_channel channel,
    n3_buffer *restrict buffer,
    _Bool reliable
) {
    struct simplex_channel_state *send_state = get_send_state(link, channel);

    struct packet p = {
        .channel = channel,
        .seq = (reliable ? next_send_sequence(send_state) : 0),
        .buffer = n3_ref_buffer(buffer),
    };

    struct timespec now;
    send_packet(socket_fd, link, 0, &p, get_time(&now));

    if(reliable)
        add_packet(&send_state->pool, &p, NULL);
    else
        destroy_packet(&p);
}

static void handle_ping(
    int socket_fd,
    struct link_state *restrict link,
    enum flags flags,
    const struct timespec *restrict now
) {
    if(!(flags & ACK))
        send_pong(socket_fd, link, now);
}

static void handle_ack(
    struct link_state *restrict link,
    const struct packet *restrict packet
) {
    struct simplex_channel_state *send_state
            = get_send_state(link, packet->channel);

    remove_packet(&send_state->pool, &packet->seq, NULL);
}

static void handle_hup(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote,
    _Bool timeout,
    void *remote_unlink_callback_data
) {
    remove_link_state(&terminal->links, remote, NULL);

    if(terminal->options.remote_unlink_callback) {
        terminal->options.remote_unlink_callback(
            terminal,
            remote,
            timeout,
            remote_unlink_callback_data
        );
    }
}

static struct packet *next_received_packet_in_channel(
    struct link_state *restrict link,
    n3_channel channel,
    struct packet *restrict packet
) {
    struct simplex_channel_state *recv_state
            = &link->ordered_states[channel - N3_ORDERED_CHANNEL_MIN].recv;
    sequence next = next_recv_sequence(recv_state->seq);
    if(recv_state->pool.count > 0 && recv_state->pool.packets[0].seq == next) {
        recv_state->seq = next;
        // TODO: remove by index instead of key.
        remove_packet(&recv_state->pool, &next, packet);
        return packet;
    }

    return NULL;
}

static struct link_state *next_received_packet(
    struct link_states *restrict links,
    struct packet *restrict packet
) {
    for(int i = 0; i < links->count; i++) {
        struct link_state *s = &links->links[i];

        for(int j = 0; j < B3_STATIC_ARRAY_COUNT(s->ordered_states); j++) {
            struct packet *p = next_received_packet_in_channel(
                s,
                j + N3_ORDERED_CHANNEL_MIN,
                packet
            );
            if(p)
                return s;
        }
    }

    return NULL;
}

static struct packet *handle_message(
    n3_terminal *restrict terminal,
    struct link_state *restrict link,
    struct packet *restrict packet,
    void *buf,
    size_t size,
    const struct timespec *restrict now
) {
    if(packet->seq != 0)
        send_ack(terminal->socket_fd, link, packet, now);

    packet->buffer = terminal->options.build_receive_buffer(
        buf,
        size,
        &terminal->options.receive_allocator
    );

    if(!N3_IS_ORDERED(packet->channel))
        return packet;

    struct duplex_channel_state *state
            = &link->ordered_states[packet->channel - N3_ORDERED_CHANNEL_MIN];
    sequence seq = packet->seq;
    add_packet(&state->recv.pool, packet, &seq);

    return next_received_packet_in_channel(link, packet->channel, packet);
}

static struct link_state *get_link(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote,
    void *new_link_filter_data
) {
    struct link_state *link = find_link_state(&terminal->links, remote);
    if(link)
        log_debug(""); // Add newline to line describing packet.
    else {
        if(terminal->filter_new_link && !terminal->filter_new_link(
            terminal,
            remote,
            new_link_filter_data
        )) {
            log_debug(", rejected new link; ignoring");
            return NULL;
        }

        link = insert_link_state(&terminal->links, remote);

        log_debug(", created new link");
    }

    return link;
}

static void log_received_from(const n3_host *restrict remote) {
    char address[N3_ADDRESS_SIZE] = {""};
    n3_get_host_address(remote, address, sizeof(address));
    n3_port port = n3_get_host_port(remote);

    log_n_debug("Received from %s|%"PRIu16": ", address, port);
}

static void log_received_packet(
    enum flags flags,
    const struct packet *restrict packet
) {
    if(flags & PING) {
        if(flags & ACK)
            log_n_debug("PONG");
        else
            log_n_debug("PING");
    }
    else if(flags & ACK)
        log_n_debug("ACK %"PRIu8"-%"PRIu16, packet->channel, packet->seq);
    else if(flags & FIN)
        log_n_debug("FIN");
    else
        log_n_debug("message %"PRIu8"-%"PRIu16, packet->channel, packet->seq);
}

static struct link_state *receive_packet(
    n3_terminal *restrict terminal,
    void *new_link_filter_data,
    void *remote_unlink_callback_data,
    struct packet *restrict packet
) {
    struct link_state *link = next_received_packet(&terminal->links, packet);
    if(link)
        return link;

    while(1) {
        uint8_t header[N3_HEADER_SIZE];
        uint8_t buf[terminal->options.max_buffer_size];
        void *bufs[] = {header, buf};
        size_t sizes[] = {sizeof(header), sizeof(buf)};

        n3_host remote;
        size_t received = n3_raw_receive(
            terminal->socket_fd,
            B3_STATIC_ARRAY_COUNT(bufs),
            bufs,
            sizes,
            &remote
        );
        if(!received)
            break;

        log_received_from(&remote);

        struct timespec now;
        get_time(&now);

        enum flags flags = 0;
        struct packet p = {.buffer = NULL};
        if(!read_proto_header(header, received, &flags, &p.channel, &p.seq))
            continue;

        log_received_packet(flags, &p);

        link = get_link(terminal, &remote, new_link_filter_data);
        if(!link)
            continue;

        link->recv_time = now;

        if(flags & PING) {
            handle_ping(terminal->socket_fd, link, flags, &now);
            continue;
        }
        if(flags & ACK) {
            handle_ack(link, &p);
            continue;
        }
        if(flags & FIN) {
            handle_hup(terminal, &remote, 0, remote_unlink_callback_data);
            continue;
        }

        struct packet *out
                = handle_message(terminal, link, &p, buf, sizes[1], &now);
        // In this case, we've received an ordered packet out of order, and
        // don't have anything to return yet.  Keep trying the network.
        if(!out)
            continue;

        *packet = *out;
        return link;
    }

    return NULL;
}

n3_buffer *receive_buffer(
    n3_terminal *restrict terminal,
    n3_channel *restrict channel,
    void *new_link_filter_data,
    void *remote_unlink_callback_data,
    struct link_state **restrict link
) {
    struct packet p = {.buffer = NULL};
    *link = receive_packet(
        terminal,
        new_link_filter_data,
        remote_unlink_callback_data,
        &p
    );
    if(!*link)
        return NULL;

    if(channel)
        *channel = p.channel;

    n3_buffer *buffer = n3_ref_buffer(p.buffer);
    destroy_packet(&p);
    return buffer;
}

static void resend_packets(
    int socket_fd,
    struct link_state *restrict link,
    struct simplex_channel_state *restrict send_state,
    const struct timespec *restrict now,
    int timeout_ms
) {
    for(int i = 0; i < send_state->pool.count; i++) {
        if(timeout_elapsed(&send_state->pool.packets[i].time, timeout_ms, now))
            send_packet(socket_fd, link, 0, &send_state->pool.packets[i], now);
    }
}

void upkeep(
    n3_terminal *restrict terminal,
    void *remote_unlink_callback_data,
    const struct timespec *restrict now
) {
    for(int i = 0; i < terminal->links.count; i++) {
        struct link_state *s = &terminal->links.links[i];

        if(timeout_elapsed(
            &s->recv_time,
            terminal->options.unlink_timeout_ms,
            now
        )) {
            handle_hup(terminal, &s->remote, 1, remote_unlink_callback_data);
            // HACK: because handle_hup removes the link from terminal->links,
            // we need to bump i back one too, so the next iteration gets the
            // correct new next link.
            i--;
            continue;
        }

        // Ping only if we aren't awaiting a response and it's been a while
        // since we last heard from them.
        if(compare_timespec(&s->send_time, &s->recv_time) < 0
                && timeout_elapsed(
                    &s->recv_time,
                    terminal->options.ping_timeout_ms,
                    now
                ))
            send_ping(terminal->socket_fd, s, now);

        // TODO: this could be a looot more efficient.  Either limit how many
        // states we keep track of, or keep track of a "next resend time" value
        // (and maybe packet pointer), so we don't have to loop as often.
        for(int j = 0; j < B3_STATIC_ARRAY_COUNT(s->ordered_states); j++) {
            resend_packets(
                terminal->socket_fd,
                s,
                &s->ordered_states[j].send,
                now,
                terminal->options.resend_timeout_ms
            );
        }
        for(int j = 0; j < B3_STATIC_ARRAY_COUNT(s->unordered_states); j++) {
            resend_packets(
                terminal->socket_fd,
                s,
                &s->unordered_states[j],
                now,
                terminal->options.resend_timeout_ms
            );
        }
    }
}
