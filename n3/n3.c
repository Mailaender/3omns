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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


#define INIT_LINK_STATES_SIZE 8

struct n3_link {
    int ref_count;
    n3_terminal *terminal;
    n3_host remote;
};


static n3_verbosity verbosity = N3_SILENT;
static FILE *log_file = NULL;


void n3_init(n3_verbosity verbosity_, FILE *restrict log_file_) {
    verbosity = verbosity_;
    log_file = log_file_;
}

void n3_quit(void) {
}

void log_(
    n3_verbosity level,
    _Bool newline,
    const char *restrict format,
    ...
) {
    if(level > verbosity || !log_file)
        return;

    va_list args;
    va_start(args, format);

    switch(level) {
    case N3_ERRORS: fputs("ERROR: ", log_file); break;
    case N3_WARNINGS: fputs("WARNING: ", log_file); break;
    default: break;
    }
    vfprintf(log_file, format, args);
    if(newline)
        fputc('\n', log_file);

    va_end(args);
}

static n3_terminal *new_terminal(
    int socket_fd,
    n3_link_filter new_link_filter,
    const n3_terminal_options *restrict options
) {
    n3_terminal *terminal = b3_malloc(sizeof(*terminal), 1);

    terminal->options.max_buffer_size = N3_SAFE_BUFFER_SIZE;
    terminal->options.resend_timeout_ms = N3_DEFAULT_RESEND_TIMEOUT_MS;
    terminal->options.ping_timeout_ms = N3_DEFAULT_PING_TIMEOUT_MS;
    terminal->options.unlink_timeout_ms = N3_DEFAULT_UNLINK_TIMEOUT_MS;
    terminal->options.build_receive_buffer = n3_build_buffer;
    if(options) {
        if(options->max_buffer_size)
            terminal->options.max_buffer_size = options->max_buffer_size;
        if(options->resend_timeout_ms > 0)
            terminal->options.resend_timeout_ms = options->resend_timeout_ms;
        if(options->ping_timeout_ms > 0)
            terminal->options.ping_timeout_ms = options->ping_timeout_ms;
        if(options->unlink_timeout_ms > 0)
            terminal->options.unlink_timeout_ms = options->unlink_timeout_ms;
        terminal->options.receive_allocator = options->receive_allocator;
        if(options->build_receive_buffer) {
            terminal->options.build_receive_buffer
                    = options->build_receive_buffer;
        }
        terminal->options.remote_unlink_callback
                = options->remote_unlink_callback;
    }

    terminal->socket_fd = socket_fd;
    terminal->filter_new_link = new_link_filter;

    init_link_states(&terminal->links, INIT_LINK_STATES_SIZE);

    return n3_ref_terminal(terminal);
}

n3_terminal *n3_new_terminal(
    const n3_host *restrict local,
    n3_link_filter new_link_filter,
    const n3_terminal_options *restrict options
) {
    return new_terminal(
        n3_new_listening_socket(local),
        new_link_filter,
        options
    );
}

n3_terminal *n3_ref_terminal(n3_terminal *restrict terminal) {
    terminal->ref_count++;
    return terminal;
}

void n3_free_terminal(n3_terminal *restrict terminal) {
    if(terminal && !--terminal->ref_count) {
        n3_unlink_from(terminal, NULL);
        if(terminal->socket_fd >= 0) {
            n3_free_socket(terminal->socket_fd);
            terminal->socket_fd = -1;
        }
        terminal->filter_new_link = NULL;
        destroy_link_states(&terminal->links);
        b3_free(terminal, 0);
    }
}

int n3_get_fd(n3_terminal *restrict terminal) {
    return terminal->socket_fd;
}

n3_host *n3_get_host(n3_terminal *restrict terminal, n3_host *restrict host) {
    return n3_init_host_from_socket_local(host, terminal->socket_fd);
}

void n3_for_each_link(
    n3_terminal *restrict terminal,
    n3_link_callback callback,
    void *data
) {
    if(terminal->links.count > 0) {
        // Copy the list so the caller can modify the links from the callback
        // without ill effect.
        n3_host remotes[terminal->links.count];
        for(int i = 0; i < terminal->links.count; i++)
            remotes[i] = terminal->links.links[i].remote;

        for(int i = 0; i < B3_STATIC_ARRAY_COUNT(remotes); i++)
            callback(terminal, &remotes[i], data);
    }
}

void n3_broadcast(
    n3_terminal *restrict terminal,
    n3_channel channel,
    n3_buffer *restrict buffer
) {
    // TODO: add unreliable option.
    for(int i = 0; i < terminal->links.count; i++) {
        send_buffer(
            terminal->socket_fd,
            &terminal->links.links[i],
            channel,
            buffer,
            1
        );
    }
}

void n3_send_to(
    n3_terminal *restrict terminal,
    n3_channel channel,
    n3_buffer *restrict buffer,
    const n3_host *restrict remote
) {
    struct link_state *link = find_link_state(&terminal->links, remote);
    if(!link)
        link = insert_link_state(&terminal->links, remote);

    // TODO: add unreliable option.
    send_buffer(terminal->socket_fd, link, channel, buffer, 1);
}

n3_buffer *n3_receive(
    n3_terminal *restrict terminal,
    n3_channel *restrict channel,
    n3_host *restrict remote,
    void *new_link_filter_data,
    void *remote_unlink_callback_data
) {
    struct link_state *link = NULL;
    n3_buffer *buffer = receive_buffer(
        terminal,
        channel,
        new_link_filter_data,
        remote_unlink_callback_data,
        &link
    );
    if(!buffer)
        return NULL;

    if(remote)
        *remote = link->remote;
    return buffer;
}

void n3_update(
    n3_terminal *restrict terminal,
    void *remote_unlink_callback_data
) {
    struct timespec now;
    upkeep(terminal, remote_unlink_callback_data, get_time(&now));
}

static void unlink_from(
    int socket_fd,
    struct link_states *restrict links,
    const struct n3_host *restrict remote,
    const struct timespec *restrict now
) {
    struct link_state *link = find_link_state(links, remote);
    if(link) {
        send_fin(socket_fd, link, now);
        // TODO: remove by index instead of key.
        remove_link_state(links, remote, NULL);
    }
}

static void unlink_all_callback(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote,
    void *data
) {
    const struct timespec *restrict now = data;

    unlink_from(terminal->socket_fd, &terminal->links, remote, now);
}

void n3_unlink_from(n3_terminal *restrict terminal, n3_host *restrict remote) {
    struct timespec now;
    get_time(&now);

    if(!remote) {
        n3_for_each_link(terminal, unlink_all_callback, &now);
        return;
    }

    unlink_from(terminal->socket_fd, &terminal->links, remote, &now);
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

    if(!find_link_state(&terminal->links, remote)) {
        struct link_state *ls = insert_link_state(&terminal->links, remote);

        struct timespec now;
        send_ping(terminal->socket_fd, ls, get_time(&now)); // Ping = connect.
    }

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

void n3_unlink(n3_link *restrict link) {
    n3_unlink_from(link->terminal, &link->remote);
}
