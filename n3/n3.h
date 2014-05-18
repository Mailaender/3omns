#ifndef __n3_h__
#define __n3_h__

#include <stddef.h>
#include <stdint.h>
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


int n3_new_server_socket(const n3_host *restrict local);
int n3_new_client_socket(const n3_host *restrict remote);
void n3_free_socket(int socket_fd);

void n3_send(
    int socket_fd,
    int buf_count,
    const uint8_t *const bufs[],
    const size_t sizes[], // Total should be <= 548.
    const n3_host *restrict remote // NULL if client.
);
size_t n3_receive(
    int socket_fd,
    int buf_count,
    uint8_t *restrict bufs[],
    size_t sizes[],
    n3_host *restrict remote // NULL if client.
);


typedef struct n3_client n3_client;

n3_client *n3_new_client(const n3_host *restrict remote);
void n3_free_client(n3_client *restrict client);

// TODO: getters: socket_fd.

void n3_client_send(
    n3_client *restrict client,
    const uint8_t *restrict buf,
    size_t size
);
size_t n3_client_receive(
    n3_client *restrict client,
    uint8_t *restrict buf,
    size_t size
);


typedef struct n3_server n3_server;

typedef _Bool (*n3_connection_filter_callback)(
    n3_server *server,
    const n3_host *host,
    void *data
);

n3_server *n3_new_server(
    const n3_host *restrict local,
    n3_connection_filter_callback connection_filter_callback
);
void n3_free_server(n3_server *restrict server);

// TODO: getters: connections, socket_fd.

void n3_server_broadcast(
    n3_server *restrict server,
    const uint8_t *restrict buf,
    size_t size
);
void n3_server_send_to(
    n3_server *restrict server,
    const uint8_t *restrict buf,
    size_t size,
    const n3_host *restrict host
);
size_t n3_server_receive(
    n3_server *restrict server,
    uint8_t *restrict buf,
    size_t size,
    n3_host *restrict host,
    void *connection_filter_data
);


#endif
