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

#ifndef n3_n3_h__
#define n3_n3_h__

#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>


// TODO: error reporting (return NULL/false and pass extra char ** for desc.).

typedef enum n3_verbosity n3_verbosity;
enum n3_verbosity {
    N3_SILENT = 0, // TODO: currently, N3_SILENT is roughly == to N3_ERRORS.
    N3_ERRORS = 1,
    N3_WARNINGS = 2,
    N3_DEBUG = 3,
};

void n3_init(n3_verbosity verbosity, FILE *restrict log_file);
void n3_quit(void);


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
// TODO: init from string ip/ipv6 representation plus n3_port, and maybe from
// another sockaddr_*?

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

#define N3_DEFAULT_RESEND_TIMEOUT_MS 500
#define N3_DEFAULT_PING_TIMEOUT_MS 1000
#define N3_DEFAULT_UNLINK_TIMEOUT_MS 3000


typedef void *(*n3_malloc)(size_t size);
typedef void (*n3_free)(void *buf, size_t size);

typedef struct n3_allocator n3_allocator;
struct n3_allocator {
    n3_malloc malloc;
    n3_free free;
};

// TODO: define a constant for how much overhead in addition to the buffer size
// each allocation will request (i.e. sizeof(n3_buffer))?
typedef struct n3_buffer n3_buffer;

n3_buffer *n3_new_buffer(size_t size, const n3_allocator *restrict allocator);
n3_buffer *n3_ref_buffer(n3_buffer *restrict buffer);
void n3_free_buffer(n3_buffer *restrict buffer);

void *n3_get_buffer(n3_buffer *restrict buffer);
size_t n3_get_buffer_size(n3_buffer *restrict buffer);
size_t n3_get_buffer_cap(n3_buffer *restrict buffer);
void n3_set_buffer_cap(n3_buffer *restrict buffer, size_t cap);


typedef uint8_t n3_channel;

#define N3_ORDERED_CHANNEL_MIN 0
#define N3_ORDERED_CHANNEL_MAX 0x7f
#define N3_UNORDERED_CHANNEL_MIN 0x80
#define N3_UNORDERED_CHANNEL_MAX 0xff

#define N3_CHANNEL_MIN N3_ORDERED_CHANNEL_MIN
#define N3_CHANNEL_MAX N3_UNORDERED_CHANNEL_MAX

#define N3_IS_ORDERED(ch) ((ch) <= N3_ORDERED_CHANNEL_MAX)


typedef struct n3_terminal n3_terminal;

typedef void (*n3_link_callback)(
    n3_terminal *terminal,
    const n3_host *remote,
    void *data
);
typedef _Bool (*n3_link_filter)(
    n3_terminal *terminal,
    const n3_host *remote,
    void *data
);
typedef void (*n3_unlink_callback)(
    n3_terminal *terminal,
    const n3_host *remote,
    _Bool timeout,
    void *data
);
typedef n3_buffer *(*n3_buffer_builder)(
    void *buf,
    size_t size,
    const n3_allocator *allocator
);

typedef struct n3_terminal_options n3_terminal_options;
struct n3_terminal_options {
    size_t max_buffer_size;
    int resend_timeout_ms;
    int ping_timeout_ms;
    int unlink_timeout_ms;
    n3_allocator receive_allocator;
    n3_buffer_builder build_receive_buffer;
    n3_unlink_callback remote_unlink_callback;
};
#define N3_TERMINAL_OPTIONS_INIT {0, 0, 0, 0, {NULL, NULL}, NULL, NULL}

n3_terminal *n3_new_terminal(
    const n3_host *restrict local,
    n3_link_filter new_link_filter,
    const n3_terminal_options *restrict options
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

void n3_broadcast(
    n3_terminal *restrict terminal,
    n3_channel channel,
    n3_buffer *restrict buffer
    // TODO: bool whether reliable at all (or maybe separate method?).
);
void n3_send_to(
    n3_terminal *restrict terminal,
    n3_channel channel,
    n3_buffer *restrict buffer,
    const n3_host *restrict remote
);
n3_buffer *n3_receive(
    n3_terminal *restrict terminal,
    n3_host *restrict remote,
    void *new_link_filter_data,
    void *remote_unlink_callback_data
);

void n3_update(
    n3_terminal *restrict terminal,
    void *remote_unlink_callback_data
);

void n3_unlink_from(n3_terminal *restrict terminal, n3_host *restrict remote);


typedef struct n3_link n3_link;

n3_link *n3_new_link(
    const n3_host *restrict remote,
    const n3_terminal_options *restrict terminal_options
);
n3_link *n3_link_to(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote
);
n3_link *n3_ref_link(n3_link *restrict link);
void n3_free_link(n3_link *restrict link);

// TODO: is_linked, that returns whether the terminal still has the link state.
n3_terminal *n3_get_terminal(n3_link *restrict link);
// TODO: getter for remote n3_host.

void n3_send(
    n3_link *restrict link,
    n3_channel channel,
    n3_buffer *restrict buffer
);

void n3_unlink(n3_link *restrict link);


#endif
