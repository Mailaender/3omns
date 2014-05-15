#include "n3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdint.h>
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

n3_host *n3_init_host_from_socket_local(n3_host *restrict host, int socket_fd) {
    socklen_t size = sizeof(host->address);
    if(getsockname(socket_fd, (struct sockaddr *)&host->address, &size) < 0)
        b3_fatal("Error getting socket address: %s", strerror(errno));
    if(size > sizeof(host->address))
        b3_fatal("Socket address too big, %'zd bytes", (ssize_t)size);
    host->size = (size_t)size;
    return host;
}

char *n3_get_host_address(
    const n3_host *restrict host,
    char *restrict address,
    size_t size
) {
    if(host->address.ss_family != AF_INET && host->address.ss_family != AF_INET6)
        snprintf(address, size, "%s", "(unknown)");
    else {
        void *addr = (host->address.ss_family == AF_INET
                ? (void *)&((struct sockaddr_in *)&host->address)->sin_addr
                : (void *)&((struct sockaddr_in6 *)&host->address)->sin6_addr);
        if(!inet_ntop(host->address.ss_family, addr, address, size))
            b3_fatal("Error converting host address to string: %s", strerror(errno));
    }

    return address;
}

n3_port n3_get_host_port(const n3_host *restrict host) {
    if(host->address.ss_family != AF_INET && host->address.ss_family != AF_INET6)
        return 0;

    n3_port nport = (host->address.ss_family == AF_INET
            ? ((struct sockaddr_in *)&host->address)->sin_port
            : ((struct sockaddr_in6 *)&host->address)->sin6_port);
    return ntohs(nport);
}

static int new_socket(_Bool server, const n3_host *restrict address) {
    int socket_fd = socket(address->address.ss_family, SOCK_DGRAM, 0);
    if(socket_fd < 0)
        b3_fatal("Error creating %s socket: %s", (server ? "server" : "client"), strerror(errno));

    if(server) {
        if(bind(socket_fd, (struct sockaddr *)&address->address, address->size) < 0)
            b3_fatal("Error binding server socket: %s", strerror(errno));
    }
    else {
        if(connect(socket_fd, (struct sockaddr *)&address->address, address->size) < 0)
            b3_fatal("Error connecting client socket: %s", strerror(errno));
        // TODO: bind to IN*ADDR_ANY at port 0, to nail down ephemeral port #?
    }

    return socket_fd;
}

int n3_new_server_socket(const n3_host *restrict local) {
    return new_socket(1, local);
}

int n3_new_client_socket(const n3_host *restrict remote) {
    return new_socket(0, remote);
}

void n3_free_socket(int socket_fd) {
    close(socket_fd);
}

void n3_send(
    int socket_fd,
    int buf_count,
    const uint8_t *const bufs[],
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
    struct msghdr msg = {
        .msg_iov = iovecs,
        .msg_iovlen = (size_t)buf_count,
    };
    if(remote) {
        msg.msg_name = (void *)&remote->address;
        msg.msg_namelen = remote->size;
    }

    ssize_t sent = sendmsg(socket_fd, &msg, MSG_DONTWAIT); // TODO: MSG_CONFIRM?
    if(sent < 0)
        b3_fatal("Error sending: %s", strerror(errno));
    if((size_t)sent != size)
        b3_fatal("Sent data truncated, %'zd of %'zu bytes", sent, size);
}

size_t n3_receive(
    int socket_fd,
    int buf_count,
    uint8_t *restrict bufs[],
    size_t sizes[],
    n3_host *restrict remote
) {
    struct iovec iovecs[buf_count];
    for(int i = 0; i < buf_count; i++) {
        iovecs[i].iov_base = bufs[i];
        iovecs[i].iov_len = sizes[i];
    }
    struct msghdr msg = {
        .msg_iov = iovecs,
        .msg_iovlen = (size_t)buf_count,
    };
    if(remote) {
        msg.msg_name = &remote->address;
        msg.msg_namelen = sizeof(remote->address);
    }

    ssize_t received = recvmsg(socket_fd, &msg, MSG_DONTWAIT);
    if(received < 0)
        b3_fatal("Error receiving: %s", strerror(errno));
    // FIXME: this introduces a remotely activatable denial of service, where
    // a malicious remote host can send a huge message, causing the local
    // executable to exit with error.  For testing, I need to know if this ever
    // happens, though, so I'm leaving it in for now.
    if(msg.msg_flags & MSG_TRUNC)
        b3_fatal("Received data truncated");

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
