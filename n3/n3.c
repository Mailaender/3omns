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
#include "b3/b3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


struct n3_buffer {
    int ref_count;
    n3_free free;
    size_t size;
    size_t cap;
    uint8_t buf[];
};

struct link_data {
    n3_host remote;
    // TODO
};

struct n3_terminal {
    int ref_count;

    n3_terminal_options options;

    int socket_fd;

    n3_link_callback filter_incoming_link;

    struct link_data *links;
    int link_size;
    int link_count;
};

struct n3_link {
    int ref_count;

    n3_terminal *terminal;
    n3_host remote;
};


static const uint8_t magic[N3_HEADER_SIZE] = {0x31,0xc6,0xa9,0xfa};


static void *default_malloc(size_t size) {
    return b3_malloc(size, 0);
}

static void default_free(void *restrict buf, size_t size) {
    b3_free(buf, 0);
}

n3_buffer *n3_new_buffer(size_t size, const n3_allocator *restrict allocator) {
    n3_malloc malloc_ = default_malloc;
    n3_free free_ = default_free;
    if(allocator) {
        if(allocator->malloc)
            malloc_ = allocator->malloc;
        if(allocator->free)
            free_ = allocator->free;
    }

    n3_buffer *buffer = malloc_(size + sizeof(*buffer));
    buffer->ref_count = 0;
    buffer->free = free_;
    buffer->size = size;
    buffer->cap = size;
    return n3_ref_buffer(buffer);
}

n3_buffer *n3_ref_buffer(n3_buffer *restrict buffer) {
    buffer->ref_count++;
    return buffer;
}

void n3_free_buffer(n3_buffer *restrict buffer) {
    if(buffer && !--buffer->ref_count) {
        n3_free free_ = buffer->free;
        size_t size = buffer->size;

        buffer->free = NULL;
        buffer->size = 0;
        buffer->cap = 0;
        free_(buffer, size + sizeof(*buffer));
    }
}

void *n3_get_buffer(n3_buffer *restrict buffer) {
    return buffer->buf;
}

size_t n3_get_buffer_size(n3_buffer *restrict buffer) {
    return buffer->size;
}

size_t n3_get_buffer_cap(n3_buffer *restrict buffer) {
    return buffer->cap;
}

void n3_set_buffer_cap(n3_buffer *restrict buffer, size_t cap) {
    if(cap <= buffer->size)
        buffer->cap = cap;
}

static struct link_data *init_link_data(
    struct link_data *link_data,
    const n3_host *restrict remote
) {
    link_data->remote = *remote;
    return link_data;
}

static void destroy_link_data(struct link_data *link_data) {
    // No-op for now.
}

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
    n3_link_callback incoming_link_filter,
    const n3_terminal_options *restrict options
) {
    n3_terminal *terminal = b3_malloc(sizeof(*terminal), 1);
    terminal->options.max_buffer_size = N3_SAFE_BUFFER_SIZE;
    terminal->options.build_receive_buffer = default_build_receive_buffer;
    if(options) {
        if(options->max_buffer_size)
            terminal->options.max_buffer_size = options->max_buffer_size;
        terminal->options.receive_allocator = options->receive_allocator;
        if(options->build_receive_buffer) {
            terminal->options.build_receive_buffer
                    = options->build_receive_buffer;
        }
    }
    terminal->socket_fd = socket_fd;
    terminal->filter_incoming_link = incoming_link_filter;
    terminal->link_size = 8;
    terminal->links
            = b3_malloc(terminal->link_size * sizeof(*terminal->links), 1);
    return n3_ref_terminal(terminal);
}

n3_terminal *n3_new_terminal(
    const n3_host *restrict local,
    n3_link_callback incoming_link_filter,
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
        if(terminal->socket_fd >= 0) {
            n3_free_socket(terminal->socket_fd);
            terminal->socket_fd = -1;
        }
        terminal->filter_incoming_link = NULL;
        for(int i = 0; i < terminal->link_count; i++)
            destroy_link_data(&terminal->links[i]);
        b3_free(terminal->links, 0);
        terminal->links = NULL;
        terminal->link_size = 0;
        terminal->link_count = 0;
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
    for(int i = 0; i < terminal->link_count; i++)
        callback(terminal, &terminal->links[i].remote, data);
}

static void proto_send(
    int socket_fd,
    n3_buffer *restrict buffer,
    const n3_host *restrict remote
) {
    // TODO: reliability.

    const void *bufs[] = {magic, buffer->buf};
    size_t sizes[] = {sizeof(magic), buffer->cap};
    n3_raw_send(socket_fd, B3_STATIC_ARRAY_COUNT(bufs), bufs, sizes, remote);
}

static size_t proto_receive(
    int socket_fd,
    void *restrict buf,
    size_t size,
    n3_host *restrict remote
) {
    uint8_t magic_test[sizeof(magic)];
    void *bufs[] = {magic_test, buf};
    size_t sizes[] = {sizeof(magic_test), size};

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
        if(sizes[0] == sizeof(magic) && !memcmp(magic_test, magic, sizes[0]))
            return sizes[1];
    }

    return 0;
}

void n3_broadcast(
    n3_terminal *restrict terminal,
    n3_buffer *restrict buffer
) {
    for(int i = 0; i < terminal->link_count; i++)
        proto_send(terminal->socket_fd, buffer, &terminal->links[i].remote);
}

static int compare_link_data(const void *a_, const void *b_) {
    const struct link_data *restrict a = a_;
    const struct link_data *restrict b = b_;
    return n3_compare_hosts(&a->remote, &b->remote);
}

static struct link_data *search_link_data(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote
) {
    return bsearch(
        remote,
        terminal->links,
        terminal->link_count,
        sizeof(*terminal->links),
        compare_link_data
    );
}

static void insert_link_data(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote
) {
    struct link_data data;
    init_link_data(&data, remote);

    if(terminal->link_count + 1 > terminal->link_size) {
        terminal->link_size *= 2;
        terminal->links = b3_realloc(
            terminal->links,
            terminal->link_size * sizeof(*terminal->links)
        );
    }

    // TODO: a binary search instead of a scan.
    int i;
    for(i = 0; i < terminal->link_count; i++) {
        if(compare_link_data(&data, &terminal->links[i]) < 0)
            break;
    }

    memmove(
        &terminal->links[i + 1],
        &terminal->links[i],
        (terminal->link_count - i) * sizeof(*terminal->links)
    );

    memcpy(&terminal->links[i], &data, sizeof(data));
    terminal->link_count++;
}

void n3_send_to(
    n3_terminal *restrict terminal,
    n3_buffer *restrict buffer,
    const n3_host *restrict remote
) {
    if(!search_link_data(terminal, remote))
        insert_link_data(terminal, remote);

    proto_send(terminal->socket_fd, buffer, remote);
}

n3_buffer *n3_receive(
    n3_terminal *restrict terminal,
    n3_host *restrict remote,
    void *incoming_link_filter_data
) {
    n3_host remote_;
    n3_host *r = (remote ? remote : &remote_);

    uint8_t receive_buf[terminal->options.max_buffer_size];
    for(
        size_t received;
        (received = proto_receive(
            terminal->socket_fd,
            receive_buf,
            sizeof(receive_buf),
            r
        )) > 0;
    ) {
        // TODO: does this link juggling need to happen inside proto_receive?
        if(!search_link_data(terminal, r)) {
            if(terminal->filter_incoming_link
                    && !terminal->filter_incoming_link(
                terminal,
                r,
                incoming_link_filter_data
            ))
                continue;

            insert_link_data(terminal, r);
        }

        return terminal->options.build_receive_buffer(
            receive_buf,
            received,
            &terminal->options.receive_allocator
        );
    }

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
    insert_link_data(terminal, remote);

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
        // TODO: disconnect.
        n3_free_terminal(link->terminal);
        b3_free(link, sizeof(*link));
    }
}

n3_terminal *n3_get_terminal(n3_link *restrict link) {
    return n3_ref_terminal(link->terminal);
}

void n3_send(
    n3_link *restrict link,
    n3_buffer *restrict buffer
) {
    n3_send_to(link->terminal, buffer, &link->remote);
}
