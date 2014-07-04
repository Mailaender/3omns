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

#ifndef __n3_h__
#define __n3_h__

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>


// TODO: error reporting (return NULL/false and pass extra char ** for desc.).


#define N3_ADDRESS_SIZE INET6_ADDRSTRLEN

typedef in_port_t n3_port;

typedef struct n3_host n3_host;
struct n3_host {
    struct sockaddr_storage address;
    socklen_t size;
};

n3_host *n3_init_host(
    n3_host *restrict host,
    const char *restrict hostname,
    n3_port port
);
n3_host *n3_init_host_any_local(n3_host *restrict host, n3_port port);
n3_host *n3_init_host_from_socket_local(n3_host *restrict host, int socket_fd);

char *n3_get_host_address(
    const n3_host *restrict host,
    char *restrict address, // Should be at least N3_ADDRESS_SIZE in size.
    size_t size
);
n3_port n3_get_host_port(const n3_host *restrict host);
int n3_compare_hosts(const n3_host *restrict a, const n3_host *restrict b);


int n3_new_listening_socket(const n3_host *restrict local);
int n3_new_linked_socket(const n3_host *restrict remote);
void n3_free_socket(int socket_fd);

void n3_raw_send(
    int socket_fd,
    int buf_count,
    const void *const bufs[],
    const size_t sizes[], // Total should generally be <= N3_SAFE_PACKET_SIZE.
    const n3_host *restrict remote // NULL if linked (i.e. not listening).
);
size_t n3_raw_receive(
    int socket_fd,
    int buf_count,
    void *restrict bufs[],
    size_t sizes[],
    n3_host *restrict remote // NULL if linked (i.e. not listening).
);


// Size of the n3 protocol header in bytes.  This amount of space is used by
// the n3 protocol in every sent packet.
#define N3_HEADER_SIZE 4

// A good maximum packet size to avoid path fragmentation.  If you know your
// path MTU is bigger, feel free to ignore this (or define it else-wise).  The
// IPv4 minimum reassembly buffer size is 576, minus 20 for the IP header,
// minus 8 for the UDP header, equals 548.  For details, see Michael Kerrisk's
// "The Linux Programming Interface" (2010) (ISBN-13: 978-1-59327-220-3), p.
// 1190, sec.  58.6.2, under "Selecting a UDP datagram size to avoid IP
// fragmentation".
#ifndef N3_SAFE_PACKET_SIZE
#define N3_SAFE_PACKET_SIZE 548
#endif

// A good maximum buffer size for an app's messages.  The app's messages are
// sent with the n3 protocol header prepended, which is accounted for here.
#define N3_SAFE_BUFFER_SIZE (N3_SAFE_PACKET_SIZE - N3_HEADER_SIZE)


typedef struct n3_terminal n3_terminal;

typedef _Bool (*n3_link_callback)(
    n3_terminal *terminal,
    const n3_host *remote,
    void *data
);

// TODO: add a free_packet callback so the user can provide an alternative to
// b3_free() when sent buffers are no longer needed.  It receives buf and size.
n3_terminal *n3_new_terminal(
    const n3_host *restrict local,
    n3_link_callback incoming_link_filter
);
n3_terminal *n3_ref_terminal(n3_terminal *restrict terminal);
void n3_free_terminal(n3_terminal *restrict terminal);

int n3_get_fd(n3_terminal *restrict terminal);
// TODO: getter for local n3_host.

void n3_for_each_link(
    n3_terminal *restrict terminal,
    n3_link_callback callback,
    void *data
);

// TODO: disconnect from a particular remote.

void n3_broadcast(
    n3_terminal *restrict terminal,
    void *restrict buf, // Must be b3_malloc'd, ownership passes to n3.
    size_t size
);
void n3_send_to(
    n3_terminal *restrict terminal,
    void *restrict buf, // Must be b3_malloc'd, ownership passes to n3.
    size_t size,
    const n3_host *restrict remote
);
size_t n3_receive(
    n3_terminal *restrict terminal,
    void *restrict buf,
    size_t size,
    n3_host *restrict remote,
    void *incoming_link_filter_data
);


typedef struct n3_link n3_link;

n3_link *n3_new_link(const n3_host *restrict remote);
n3_link *n3_link_to(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote
);
n3_link *n3_ref_link(n3_link *restrict link);
void n3_free_link(n3_link *restrict link);

n3_terminal *n3_get_terminal(n3_link *restrict link);
// TODO: getter for remote n3_host.
// TODO: a disconnect function separate from free?

void n3_send(
    n3_link *restrict link,
    void *restrict buf, // Must be malloc'd, ownership passes to n3.
    size_t size
);


#endif
