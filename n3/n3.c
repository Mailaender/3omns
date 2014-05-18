#include "n3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>


struct connection {
    n3_host remote;
    // TODO
};

struct n3_client {
    int socket_fd;
    struct connection connection;
};

struct n3_server {
    int socket_fd;
    n3_connection_filter_callback filter_connection;
    struct connection *connections;
    int connection_size;
    int connection_count;
};


static const uint8_t magic[8] = {0x31,0xc6,0xa9,0xfa,0x99,0xbe,0xea,0x9b};


static struct connection *init_connection(
    struct connection *connection,
    const n3_host *restrict remote
) {
    connection->remote = *remote;
    return connection;
}

n3_client *n3_new_client(const n3_host *restrict remote) {
    n3_client *client = b3_malloc(sizeof(*client), 1);
    client->socket_fd = n3_new_client_socket(remote);
    init_connection(&client->connection, remote);

    // TODO: make a "connection".

    return client;
}

void n3_free_client(n3_client *restrict client) {
    if(client) {
        if(client->socket_fd >= 0) {
            n3_free_socket(client->socket_fd);
            client->socket_fd = -1;
        }
        b3_free(client, 0);
    }
}

static void proto_send(
    int socket_fd,
    const uint8_t *restrict buf,
    size_t size,
    const n3_host *restrict host
) {
    // TODO: reliability.

    const uint8_t *bufs[] = {magic, buf};
    size_t sizes[] = {sizeof(magic), size};
    n3_send(socket_fd, B3_STATIC_ARRAY_COUNT(bufs), bufs, sizes, host);
}

static size_t proto_receive(
    int socket_fd,
    uint8_t *restrict buf,
    size_t size,
    n3_host *restrict host
) {
    uint8_t magic_test[sizeof(magic)];
    uint8_t *bufs[] = {magic_test, buf};
    size_t sizes[] = {sizeof(magic_test), size};

    for(
        size_t received;
        (received = n3_receive(
            socket_fd,
            B3_STATIC_ARRAY_COUNT(bufs),
            bufs,
            sizes,
            host
        )) > 0;
    ) {
        if(sizes[0] == sizeof(magic) && !memcmp(magic_test, magic, sizes[0]))
            return sizes[1];
    }

    return 0;
}

void n3_client_send(
    n3_client *restrict client,
    const uint8_t *restrict buf,
    size_t size
) {
    proto_send(client->socket_fd, buf, size, NULL);
}

size_t n3_client_receive(
    n3_client *restrict client,
    uint8_t *restrict buf,
    size_t size
) {
    // TODO: is it safe to assume we can only receive from the connect()ed
    // socket?  It would seem so, if the blurb in the man page is trustworthy.

    return proto_receive(client->socket_fd, buf, size, NULL);
}

n3_server *n3_new_server(
    const n3_host *restrict local,
    n3_connection_filter_callback connection_filter_callback
) {
    n3_server *server = b3_malloc(sizeof(*server), 1);
    server->socket_fd = n3_new_server_socket(local);
    server->filter_connection = connection_filter_callback;
    server->connection_size = 8;
    server->connections = b3_malloc(
        server->connection_size * sizeof(*server->connections),
        1
    );
    return server;
}

void n3_free_server(n3_server *restrict server) {
    if(server) {
        if(server->socket_fd >= 0) {
            n3_free_socket(server->socket_fd);
            server->socket_fd = -1;
        }
        server->filter_connection = NULL;
        b3_free(server->connections, 0);
        server->connections = NULL;
        server->connection_size = 0;
        server->connection_count = 0;
        b3_free(server, 0);
    }
}

void n3_server_broadcast(
    n3_server *restrict server,
    const uint8_t *restrict buf,
    size_t size
) {
    for(int i = 0; i < server->connection_count; i++) {
        proto_send(
            server->socket_fd,
            buf,
            size,
            &server->connections[i].remote
        );
    }
}

void n3_server_send_to(
    n3_server *restrict server,
    const uint8_t *restrict buf,
    size_t size,
    const n3_host *restrict host
) {
    // TODO: ensure host is an endpoint we're connected to?

    proto_send(server->socket_fd, buf, size, host);
}

static int compare_connections(const void *a_, const void *b_) {
    const struct connection *restrict a = a_;
    const struct connection *restrict b = b_;
    return n3_compare_hosts(&a->remote, &b->remote);
}

static struct connection *search_connections(
    n3_server *restrict server,
    const n3_host *restrict host
) {
    return bsearch(
        host,
        server->connections,
        server->connection_count,
        sizeof(server->connections[0]),
        compare_connections
    );
}

static void insert_connection(
    n3_server *restrict server,
    const n3_host *restrict host
) {
    struct connection c;
    init_connection(&c, host);

    if(server->connection_count + 1 > server->connection_size) {
        server->connection_size *= 2;
        server->connections = b3_realloc(
            server->connections,
            server->connection_size * sizeof(*server->connections)
        );
    }

    // TODO: a binary search instead of a scan.
    int i;
    for(i = 0; i < server->connection_count; i++) {
        if(compare_connections(&c, &server->connections[i]) < 0)
            break;
    }

    memmove(
        &server->connections[i + 1],
        &server->connections[i],
        (server->connection_count - i) * sizeof(*server->connections)
    );

    memcpy(&server->connections[i], &c, sizeof(c));
    server->connection_count++;
}

size_t n3_server_receive(
    n3_server *restrict server,
    uint8_t *restrict buf,
    size_t size,
    n3_host *restrict host,
    void *connection_filter_data
) {
    n3_host received_host;
    n3_host *restrict r_host = (host ? host : &received_host);

    for(
        size_t received;
        (received = proto_receive(server->socket_fd, buf, size, r_host)) > 0;
    ) {
        if(!search_connections(server, r_host)) {
            if(server->filter_connection && !server->filter_connection(
                server,
                r_host,
                connection_filter_data
            ))
                continue;

            insert_connection(server, r_host);
        }

        return received;
    }

    return 0;
}