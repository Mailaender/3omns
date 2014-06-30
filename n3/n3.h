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


int n3_new_listening_socket(const n3_host *restrict local_host);
int n3_new_connected_socket(const n3_host *restrict remote);
void n3_free_socket(int socket_fd);

void n3_raw_send(
    int socket_fd,
    int buf_count,
    const void *const bufs[],
    const size_t sizes[], // Total should be <= 548 (see N3_SAFE_MESSAGE_SIZE).
    const n3_host *restrict remote // NULL if connected (i.e. not listening).
);
size_t n3_raw_receive(
    int socket_fd,
    int buf_count,
    void *restrict bufs[],
    size_t sizes[],
    n3_host *restrict remote // NULL if connected (i.e. not listening).
);


// Size of the n3 protocol header in bytes.  This amount of space is used by
// the n3 protocol in every sent packet.
#define N3_HEADER_SIZE 8

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


typedef struct n3_client n3_client;

n3_client *n3_new_client(const n3_host *restrict remote);
void n3_free_client(n3_client *restrict client);

int n3_get_client_fd(n3_client *restrict client);
// TODO: getter for local n3_host.

void n3_client_send(
    n3_client *restrict client,
    const void *restrict buf,
    size_t size
);
size_t n3_client_receive(
    n3_client *restrict client,
    void *restrict buf,
    size_t size
);


typedef struct n3_server n3_server;

typedef _Bool (*n3_connection_callback)(
    n3_server *server,
    const n3_host *host,
    void *data
);

n3_server *n3_new_server(
    const n3_host *restrict local,
    n3_connection_callback connection_filter_callback
);
void n3_free_server(n3_server *restrict server);

int n3_get_server_fd(n3_server *restrict server);

void n3_for_each_connection(
    n3_server *restrict server,
    n3_connection_callback callback,
    void *data
);

void n3_broadcast(
    n3_server *restrict server,
    const void *restrict buf,
    size_t size
);
void n3_send_to(
    n3_server *restrict server,
    const void *restrict buf,
    size_t size,
    const n3_host *restrict host
);
size_t n3_server_receive(
    n3_server *restrict server,
    void *restrict buf,
    size_t size,
    n3_host *restrict host,
    void *connection_filter_data
);


#endif
