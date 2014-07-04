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


struct link_data {
    n3_host remote;
    // TODO
};

struct n3_terminal {
    int ref_count;

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

static n3_terminal *new_terminal(
    int socket_fd,
    n3_link_callback incoming_link_filter
) {
    n3_terminal *terminal = b3_malloc(sizeof(*terminal), 1);
    terminal->socket_fd = socket_fd;
    terminal->filter_incoming_link = incoming_link_filter;
    terminal->link_size = 8;
    terminal->links
            = b3_malloc(terminal->link_size * sizeof(*terminal->links), 1);
    return n3_ref_terminal(terminal);
}

n3_terminal *n3_new_terminal(
    const n3_host *restrict local,
    n3_link_callback incoming_link_filter
) {
    return new_terminal(n3_new_listening_socket(local), incoming_link_filter);
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
    const void *restrict buf,
    size_t size,
    const n3_host *restrict remote
) {
    // TODO: reliability.

    const void *bufs[] = {magic, buf};
    size_t sizes[] = {sizeof(magic), size};
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
    void *restrict buf,
    size_t size
) {
    for(int i = 0; i < terminal->link_count; i++)
        proto_send(terminal->socket_fd, buf, size, &terminal->links[i].remote);

    b3_free(buf, size);
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
    void *restrict buf,
    size_t size,
    const n3_host *restrict remote
) {
    if(!search_link_data(terminal, remote))
        insert_link_data(terminal, remote);

    proto_send(terminal->socket_fd, buf, size, remote);

    b3_free(buf, size);
}

size_t n3_receive(
    n3_terminal *restrict terminal,
    void *restrict buf,
    size_t size,
    n3_host *restrict remote,
    void *incoming_link_filter_data
) {
    n3_host remote_;
    n3_host *restrict r = (remote ? remote : &remote_);

    for(
        size_t received;
        (received = proto_receive(terminal->socket_fd, buf, size, r)) > 0;
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

        return received;
    }

    return 0;
}

static _Bool deny_new_links(
    n3_terminal *terminal,
    const n3_host *remote,
    void *data
) {
    return 0;
}

n3_link *n3_new_link(const n3_host *restrict remote) {
    n3_terminal *terminal = new_terminal(
        n3_new_linked_socket(remote),
        deny_new_links
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
    void *restrict buf,
    size_t size
) {
    n3_send_to(link->terminal, buf, size, &link->remote);
}
