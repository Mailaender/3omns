#ifndef __n3_h__
#define __n3_h__

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>


// TODO: error reporting (return NULL/false and pass extra char ** for desc.).


typedef struct n3_host n3_host;
struct n3_host {
    struct sockaddr_storage address;
    socklen_t size;
};

n3_host *n3_init_host(
    n3_host *restrict host,
    const char *restrict hostname,
    uint16_t port
);
n3_host *n3_init_host_any_local(n3_host *restrict host, uint16_t port);

char *n3_host_to_string(const n3_host *restrict host);


int n3_new_server_socket(const n3_host *restrict local);
int n3_new_client_socket(const n3_host *restrict remote);
void n3_free_socket(int sd);

// TODO: get local address.

void n3_send(
    int sd,
    int buf_count,
    const uint8_t *const bufs[],
    const size_t sizes[], // Total should be <= 548.
    const n3_host *restrict remote // NULL if client.
);
size_t n3_receive(
    int sd,
    int buf_count,
    uint8_t *restrict bufs[],
    size_t sizes[],
    n3_host *restrict remote // NULL if client.
);


#endif
