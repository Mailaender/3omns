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

#include "b3/b3.h"
#include "n3/n3.h"
#include "tests/test.h"

#include <assert.h>
#include <netinet/in.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


// FIXME: find a port that's not in use instead of hard-coding it.
static const n3_port port = 12345;

static const char server_send_data[] = {"abcdefgh"};
static const size_t server_send_size
        = B3_STATIC_ARRAY_COUNT(server_send_data) - 1;
static const char client_send_data[] = {"ijklm"};
static const size_t client_send_size
        = B3_STATIC_ARRAY_COUNT(client_send_data) - 1;


static int wait_for_read(int fd) {
    return poll(&(struct pollfd){.fd = fd, .events = POLLIN}, 1, 1000);
}

static void server(int notify_fd) {
    n3_host listen;
    n3_init_host_any_local(&listen, port);

    int sd = n3_new_listening_socket(&listen);

    uint8_t c = 1;
    ssize_t notify_written = write(notify_fd, &c, 1);
    assert(notify_written == 1);

    n3_host test_listen;
    n3_init_host_from_socket_local(&test_listen, sd);
    test_assert(n3_get_host_port(&test_listen) == port,
            "server port correct");

    int poll_rc = wait_for_read(sd);
    assert(poll_rc == 1);

    uint8_t receive_buf0[3];
    uint8_t receive_buf1[20];
    void *receive_bufs[2] = {receive_buf0, receive_buf1};
    size_t receive_sizes[2] = {sizeof(receive_buf0), sizeof(receive_buf1)};
    n3_host received_host;
    size_t received_size = n3_raw_receive(
        sd,
        2,
        receive_bufs,
        receive_sizes,
        &received_host
    );

    test_assert(received_size == client_send_size,
            "total received sizes match");
    test_assert(receive_sizes[0] == sizeof(receive_buf0),
            "first received buffer size matches");
    test_assert(receive_sizes[1] == client_send_size - sizeof(receive_buf0),
            "second received buffer size matches");

    char received_data[client_send_size + 1];
    memcpy(&received_data[0], receive_buf0, receive_sizes[0]);
    memcpy(&received_data[receive_sizes[0]], receive_buf1, receive_sizes[1]);
    received_data[sizeof(received_data) - 1] = '\0';
    test_assert(!strcmp(received_data, client_send_data),
            "received data matches sent data");

    test_assert(received_host.address.ss_family == AF_INET
            || received_host.address.ss_family == AF_INET6,
            "received from IP address");
    if(received_host.address.ss_family == AF_INET) {
        test_assert(received_host.size == sizeof(struct sockaddr_in),
                "received IPv4 address size correct");
    }
    else {
        test_assert(received_host.size == sizeof(struct sockaddr_in6),
                "received IPv6 address size correct");
    }

    uint8_t send_buf0[5];
    uint8_t send_buf1[20];
    const void *send_bufs[2] = {send_buf0, send_buf1};
    size_t send_sizes[2] = {
        sizeof(send_buf0),
        server_send_size - sizeof(send_buf0),
    };
    memcpy(send_buf0, &server_send_data[0], send_sizes[0]);
    memcpy(send_buf1, &server_send_data[send_sizes[0]], send_sizes[1]);

    n3_raw_send(sd, 2, send_bufs, send_sizes, &received_host);

    n3_free_socket(sd);
}

static void client(int wait_fd) {
    n3_host connect;
    n3_init_host(&connect, "localhost", port);

    int wait_rc = wait_for_read(wait_fd);
    assert(wait_rc == 1);

    int sd = n3_new_linked_socket(&connect);

    n3_host test_connect;
    n3_init_host_from_socket_local(&test_connect, sd);
    test_assert(n3_get_host_port(&test_connect) != 0,
            "client port immediately available");

    uint8_t send_buf0[2];
    uint8_t send_buf1[20];
    uint8_t send_buf2[1] = {'x'};
    const void *send_bufs[3] = {send_buf0, send_buf1, send_buf2};
    size_t send_sizes[3] = {
        sizeof(send_buf0),
        client_send_size - sizeof(send_buf0),
        0,
    };
    memcpy(send_buf0, &client_send_data[0], send_sizes[0]);
    memcpy(send_buf1, &client_send_data[send_sizes[0]], send_sizes[1]);

    n3_raw_send(sd, 3, send_bufs, send_sizes, NULL);

    int poll_rc = wait_for_read(sd);
    assert(poll_rc == 1);

    uint8_t receive_buf0[4];
    uint8_t receive_buf1[20];
    uint8_t receive_buf2[1];
    void *receive_bufs[3] = {receive_buf0, receive_buf1, receive_buf2};
    size_t receive_sizes[3] = {
        sizeof(receive_buf0),
        sizeof(receive_buf1),
        sizeof(receive_buf2),
    };
    n3_host received_host;
    size_t received_size = n3_raw_receive(
        sd,
        3,
        receive_bufs,
        receive_sizes,
        &received_host
    );

    test_assert(received_size == server_send_size,
            "total received sizes match");
    test_assert(receive_sizes[0] == sizeof(receive_buf0),
            "first received buffer size matches");
    test_assert(receive_sizes[1] == server_send_size - sizeof(receive_buf0),
            "second received buffer size matches");
    test_assert(receive_sizes[2] == 0, "third received buffer empty");

    char received_data[server_send_size + 1];
    memcpy(&received_data[0], receive_buf0, receive_sizes[0]);
    memcpy(&received_data[receive_sizes[0]], receive_buf1, receive_sizes[1]);
    received_data[sizeof(received_data) - 1] = '\0';
    test_assert(!strcmp(received_data, server_send_data),
            "received data matches sent data");

    test_assert(!n3_compare_hosts(&received_host, &connect),
            "received from linked host");

    n3_free_socket(sd);
}

int main(void) {
    int pipefds[2];
    int pipe_rc = pipe(pipefds);
    assert(pipe_rc >= 0);

    pid_t child_pid = fork();
    assert(child_pid >= 0);
    if(child_pid) {
        close(pipefds[0]);
        server(pipefds[1]);
        close(pipefds[1]);

        int child_status = 0;
        pid_t waited_pid = wait(&child_status);
        assert(waited_pid == child_pid);
        assert(child_status == 0);
        return 0;
    }
    else {
        close(pipefds[1]);
        client(pipefds[0]);
        close(pipefds[0]);
        return 0;
    }
}
