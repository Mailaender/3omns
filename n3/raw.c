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
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static void resolve(
    n3_host *restrict host,
    const char *restrict hostname,
    n3_port port
) {
    char service[6];
    snprintf(service, sizeof(service), "%"PRIu16, port);

    struct addrinfo hints = {
        .ai_flags = AI_V4MAPPED | AI_ADDRCONFIG,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    if(!hostname)
        hints.ai_flags |= AI_PASSIVE;

    struct addrinfo *result;
    int err = getaddrinfo(hostname, service, &hints, &result);
    if(err) {
        const char *str = gai_strerror(err);
        if(hostname)
            b3_fatal("Error looking up hostname '%s': %s", hostname, str);
        else
            b3_fatal("Error finding local wildcard address: %s", str);
    }

    // Ignore all but first result.
    memcpy(&host->address, result->ai_addr, result->ai_addrlen);
    host->size = result->ai_addrlen;

    freeaddrinfo(result);
}

n3_host *n3_init_host(
    n3_host *restrict host,
    const char *restrict hostname,
    n3_port port
) {
    resolve(host, hostname, port);
    return host;
}

n3_host *n3_init_host_any_local(n3_host *restrict host, n3_port port) {
    resolve(host, NULL, port);
    return host;
}

n3_host *n3_init_host_from_socket_local(
    n3_host *restrict host,
    int socket_fd
) {
    socklen_t size = sizeof(host->address);
    if(getsockname(socket_fd, (struct sockaddr *)&host->address, &size) < 0)
        b3_fatal("Error getting socket address: %s", strerror(errno));
    if(size > sizeof(host->address))
        b3_fatal("Socket address too big, %'zu bytes", (size_t)size);
    host->size = size;
    return host;
}

static inline in_port_t *get_nport(const n3_host *restrict host) {
    // Assume either AF_INET or AF_INET6.
    return (host->address.ss_family == AF_INET
            ? &((struct sockaddr_in *)&host->address)->sin_port
            : &((struct sockaddr_in6 *)&host->address)->sin6_port);
}

static inline void *get_addr(const n3_host *restrict host) {
    // Assume either AF_INET or AF_INET6.
    return (host->address.ss_family == AF_INET
            ? (void *)&((struct sockaddr_in *)&host->address)->sin_addr
            : (void *)&((struct sockaddr_in6 *)&host->address)->sin6_addr);
}

static inline size_t get_addr_size(const n3_host *restrict host) {
    // Assume either AF_INET or AF_INET6.
    return (host->address.ss_family == AF_INET
            ? sizeof(struct in_addr)
            : sizeof(struct in6_addr));
}

char *n3_get_host_address(
    const n3_host *restrict host,
    char *restrict address,
    size_t size
) {
    if(host->address.ss_family != AF_INET
            && host->address.ss_family != AF_INET6)
        snprintf(address, size, "%s", "(unknown)");
    else {
        if(!inet_ntop(
            host->address.ss_family,
            get_addr(host),
            address,
            size
        )) {
            b3_fatal(
                "Error converting host address to string: %s",
                strerror(errno)
            );
        }
    }

    return address;
}

n3_port n3_get_host_port(const n3_host *restrict host) {
    if(host->address.ss_family != AF_INET
            && host->address.ss_family != AF_INET6)
        return 0;

    return ntohs(*get_nport(host));
}

int n3_compare_hosts(const n3_host *restrict a, const n3_host *restrict b) {
    if(a->address.ss_family != b->address.ss_family)
        return (a->address.ss_family < b->address.ss_family ? -1 : 1);

    // Assume either AF_INET or AF_INET6.
    in_port_t a_nport = *get_nport(a);
    in_port_t b_nport = *get_nport(b);
    if(a_nport != b_nport)
        return (a_nport < b_nport ? -1 : 1); // Ignore byte ordering.

    // TODO: do IPv6's sin6_flowinfo or sin6_scope_id fields factor into this?

    return memcmp(get_addr(a), get_addr(b), get_addr_size(a));
}

static int new_socket(sa_family_t family) {
    int socket_fd = socket(family, SOCK_DGRAM, 0);
    if(socket_fd < 0)
        b3_fatal("Error creating socket: %s", strerror(errno));
    return socket_fd;
}

int n3_new_listening_socket(const n3_host *restrict local) {
    int sd = new_socket(local->address.ss_family);
    if(bind(sd, (struct sockaddr *)&local->address, local->size) < 0)
        b3_fatal("Error binding socket: %s", strerror(errno));
    return sd;
}

int n3_new_linked_socket(const n3_host *restrict remote) {
    int sd = new_socket(remote->address.ss_family);
    if(connect(sd, (struct sockaddr *)&remote->address, remote->size) < 0)
        b3_fatal("Error connecting socket: %s", strerror(errno));
    return sd;
}

void n3_free_socket(int socket_fd) {
    close(socket_fd);
}

void n3_raw_send(
    int socket_fd,
    int buf_count,
    const void *const bufs[],
    const size_t sizes[],
    const n3_host *restrict remote
) {
    size_t size = 0;
    struct iovec iovecs[buf_count];
    for(int i = 0; i < buf_count; i++) {
        iovecs[i].iov_base = (void *)bufs[i];
        iovecs[i].iov_len = sizes[i];
        size += sizes[i];
    }
    struct msghdr msg = {.msg_iov = iovecs, .msg_iovlen = (size_t)buf_count};
    if(remote) {
        msg.msg_name = (void *)&remote->address;
        msg.msg_namelen = remote->size;
    }

    // TODO: MSG_CONFIRM?
    ssize_t sent = sendmsg(socket_fd, &msg, MSG_DONTWAIT);
    if(sent < 0)
        b3_fatal("Error sending: %s", strerror(errno));
    if((size_t)sent != size)
        b3_fatal("Sent data truncated, %'zd of %'zu bytes", sent, size);
}

size_t n3_raw_receive(
    int socket_fd,
    int buf_count,
    void *restrict bufs[],
    size_t sizes[],
    n3_host *restrict remote
) {
    struct iovec iovecs[buf_count];
    for(int i = 0; i < buf_count; i++) {
        iovecs[i].iov_base = bufs[i];
        iovecs[i].iov_len = sizes[i];
    }
    struct msghdr msg = {.msg_iov = iovecs, .msg_iovlen = (size_t)buf_count};
    if(remote) {
        msg.msg_name = &remote->address;
        msg.msg_namelen = sizeof(remote->address);
    }

    ssize_t received = recvmsg(socket_fd, &msg, MSG_DONTWAIT);
    if(received < 0) {
        if(errno == EAGAIN)
            return 0;
        b3_fatal("Error receiving: %s", strerror(errno));
    }
    // FIXME: this introduces a remotely activatable denial of service, where
    // a malicious remote host can send a huge message, causing the local
    // executable to exit with error.  For testing, I need to know if this ever
    // happens, though, so I'm leaving it in for now.
    if(msg.msg_flags & MSG_TRUNC)
        b3_fatal("Received data truncated, %'zd bytes", received);

    size_t remainder = (size_t)received;
    for(int i = 0; i < buf_count; i++) {
        size_t in_buf = (remainder > sizes[i] ? sizes[i] : remainder);
        sizes[i] = in_buf;
        remainder -= in_buf;
    }
    if(remote)
        remote->size = msg.msg_namelen;

    return (size_t)received;
}
