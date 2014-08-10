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

#include "b3/b3.h"
#include "internal.h"
#include "n3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>


#define INIT_LINK_STATES_SIZE 8

struct n3_terminal {
    int ref_count;
    n3_terminal_options options;
    int socket_fd;
    n3_link_filter filter_incoming_link;
    struct link_states links;
};

struct n3_link {
    int ref_count;
    n3_terminal *terminal;
    n3_host remote;
};


static n3_buffer *default_build_receive_buffer(
    void *buf,
    size_t size,
    const n3_allocator *allocator
) {
    n3_buffer *buffer = n3_new_buffer(size, allocator);
    memcpy(buffer->buf, buf, size);
    return buffer;
}

static n3_terminal *new_terminal(
    int socket_fd,
    n3_link_filter incoming_link_filter,
    const n3_terminal_options *restrict options
) {
    n3_terminal *terminal = b3_malloc(sizeof(*terminal), 1);

    terminal->options.max_buffer_size = N3_SAFE_BUFFER_SIZE;
    terminal->options.resend_timeout_ms = N3_DEFAULT_RESEND_TIMEOUT_MS;
    terminal->options.build_receive_buffer = default_build_receive_buffer;
    if(options) {
        if(options->max_buffer_size)
            terminal->options.max_buffer_size = options->max_buffer_size;
        if(options->resend_timeout_ms > 0)
            terminal->options.resend_timeout_ms = options->resend_timeout_ms;
        terminal->options.receive_allocator = options->receive_allocator;
        if(options->build_receive_buffer) {
            terminal->options.build_receive_buffer
                    = options->build_receive_buffer;
        }
    }

    terminal->socket_fd = socket_fd;
    terminal->filter_incoming_link = incoming_link_filter;

    init_link_states(&terminal->links, INIT_LINK_STATES_SIZE);

    return n3_ref_terminal(terminal);
}

n3_terminal *n3_new_terminal(
    const n3_host *restrict local,
    n3_link_filter incoming_link_filter,
    const n3_terminal_options *restrict options
) {
    return new_terminal(
        n3_new_listening_socket(local),
        incoming_link_filter,
        options
    );
}

n3_terminal *n3_ref_terminal(n3_terminal *restrict terminal) {
    terminal->ref_count++;
    return terminal;
}

void n3_free_terminal(n3_terminal *restrict terminal) {
    if(terminal && !--terminal->ref_count) {
        // TODO: disconnect links.
        if(terminal->socket_fd >= 0) {
            n3_free_socket(terminal->socket_fd);
            terminal->socket_fd = -1;
        }
        terminal->filter_incoming_link = NULL;
        destroy_link_states(&terminal->links);
        b3_free(terminal, 0);
    }
}

int n3_get_fd(n3_terminal *restrict terminal) {
    return terminal->socket_fd;
}

void n3_for_each_link(
    n3_terminal *restrict terminal,
    n3_link_callback callback,
    void *data
) {
    // TODO: bad news here if they modify links within their callback.
    FOR_EACH_LINK_STATE(state, &terminal->links)
        callback(terminal, &state->remote, data);
}

void n3_broadcast(
    n3_terminal *restrict terminal,
    n3_channel channel,
    n3_buffer *restrict buffer
) {
    // TODO: add unreliable option.
    FOR_EACH_LINK_STATE(state, &terminal->links)
        send_buffer(terminal->socket_fd, state, channel, buffer, 1);
}

static struct link_state *insert_link_state(
    struct link_states *restrict states,
    const n3_host *restrict remote
) {
    struct link_state state;
    return add_link_state(states, init_link_state(&state, remote), remote);
}

void n3_send_to(
    n3_terminal *restrict terminal,
    n3_channel channel,
    n3_buffer *restrict buffer,
    const n3_host *restrict remote
) {
    struct link_state *state = find_link_state(&terminal->links, remote);
    if(!state)
        state = insert_link_state(&terminal->links, remote);

    // TODO: add unreliable option.
    send_buffer(terminal->socket_fd, state, channel, buffer, 1);
}

n3_buffer *n3_receive(
    n3_terminal *restrict terminal,
    n3_host *restrict remote,
    void *incoming_link_filter_data
) {
    n3_host remote_;
    n3_host *r = (remote ? remote : &remote_);

    uint8_t receive_buf[terminal->options.max_buffer_size];
    while(1) {
        enum flags flags = 0;
        struct packet p = {.buffer = NULL};
        size_t received;

        received = receive_packet(
            terminal->socket_fd,
            &flags,
            &p,
            receive_buf,
            sizeof(receive_buf),
            r
        );
        if(!received)
            break;

        struct link_state *state = find_link_state(&terminal->links, r);
        if(!state) {
            if(terminal->filter_incoming_link
                    && !terminal->filter_incoming_link(
                terminal,
                r,
                incoming_link_filter_data
            ))
                continue;

            state = insert_link_state(&terminal->links, r);
        }

        if(flags & ACK) {
            handle_ack_packet(state, &p);
            continue;
        }
        else if(flags & FIN) {
            // TODO: drop the link, notify the caller somehow.
            continue;
        }
        else { // No interesting flags.
            if(p.seq != 0)
                send_ack(terminal->socket_fd, &p, r);
        }

        // TODO: timeout when packets sit un-ack'd too long (drop link,
        // notify?).

        // TODO: if ordered, don't return it directly, but add it to the queue,
        // then start returning everything sequential from the queue.
        return terminal->options.build_receive_buffer(
            receive_buf,
            received,
            &terminal->options.receive_allocator
        );
    }

    // TODO: putting this down here means a huge burst of incoming packets
    // could prevent us from trying to resend anything...
    resend(
        terminal->socket_fd,
        terminal->options.resend_timeout_ms,
        &terminal->links
    );

    return NULL;
}

static _Bool deny_new_links(
    n3_terminal *terminal,
    const n3_host *remote,
    void *data
) {
    return 0;
}

n3_link *n3_new_link(
    const n3_host *restrict remote,
    const n3_terminal_options *restrict terminal_options
) {
    n3_terminal *terminal = new_terminal(
        n3_new_linked_socket(remote),
        deny_new_links,
        terminal_options
    );
    insert_link_state(&terminal->links, remote);

    n3_link *link = n3_link_to(terminal, remote);
    n3_free_terminal(terminal);
    return link;
}

n3_link *n3_link_to(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote
) {
    n3_link *link = b3_malloc(sizeof(*link), 1);
    link->terminal = n3_ref_terminal(terminal);
    link->remote = *remote;
    return n3_ref_link(link);
}

n3_link *n3_ref_link(n3_link *restrict link) {
    link->ref_count++;
    return link;
}

void n3_free_link(n3_link *restrict link) {
    if(link && !--link->ref_count) {
        n3_free_terminal(link->terminal);
        b3_free(link, sizeof(*link));
    }
}

n3_terminal *n3_get_terminal(n3_link *restrict link) {
    return n3_ref_terminal(link->terminal);
}

void n3_send(
    n3_link *restrict link,
    n3_channel channel,
    n3_buffer *restrict buffer
) {
    n3_send_to(link->terminal, channel, buffer, &link->remote);
}
