/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <https://chazomaticus.github.io/3omns/>
    n3 - net communication library for 3omns
    Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

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

#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define UPDATE_TIMEOUT_MS 100

struct args {
    _Bool listen;
    const char *hostname;
    const char *port;
    _Bool broadcast;
    _Bool identify;
    n3_verbosity verbosity;
};
#define ARGS_INIT_DEFAULT {0, NULL, NULL, 0, 0, N3_SILENT}

struct state {
    const struct args *args;

    n3_host remote_host; // Not used if listening.
    n3_terminal *terminal;

    int exit_status;
};
#define STATE_INIT {.args = NULL}

struct unlink {
    _Bool unlinked;
    _Bool timeout;
};
#define UNLINK_INIT {0, 0}

struct rebroadcast {
    n3_host *from_host;
    n3_buffer *buffer;
};


const char *argp_program_version = "n3c 0.1";
const char *argp_program_bug_address = "<"PACKAGE_BUGREPORT">";


static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct args *restrict args = state->input;

    switch(key) {
    case 'l': args->listen = 1; break;
    case 'v': args->verbosity++; break;
    case 'b': args->broadcast = 1; break;
    case 'i': args->identify = 1; break;

    case ARGP_KEY_ARG:
        switch(state->arg_num) {
        case 0: args->hostname = arg; break;
        case 1: args->port = arg; break;
        default:
            return ARGP_ERR_UNKNOWN;
        }
        break;

    case ARGP_KEY_END:
        // With -l, hostname may be omitted.  In that case, we've stored the
        // port in args->hostname.
        if(!args->port) {
            args->port = args->hostname;
            args->hostname = NULL;
        }

        if(!args->port || (!args->listen && !args->hostname))
            argp_usage(state);
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static void parse_args(struct args *restrict args, int argc, char *argv[]) {
    const char *const doc = "nc-like utility using the n3 protocol"
            "\vConnect to HOST:PORT; or with -l, listen for connections on "
            "BIND:PORT (default BIND: 0.0.0.0).  Send data from stdin to "
            "connection(s).  Display received data on stdout.";
    const char *const args_doc = "HOST PORT"
            "\n-l [BIND] PORT";

    struct argp_option options[] = {
        {"listen", 'l', NULL, OPTION_NO_USAGE,
                "Listen for incoming connections"},
        {"broadcast", 'b', NULL, 0, "With -l, broadcast received data"},
        {"identify", 'i', NULL, 0, "With -l, display received data's sender"},
        {"verbose", 'v', NULL, 0, "Increase verbosity (multiple allowed)"},
        {0}
    };
    struct argp argp = {options, parse_opt, args_doc, doc};

    error_t e = argp_parse(&argp, argc, argv, 0, NULL, args);
    if(e)
        b3_fatal("Error parsing arguments: %s", strerror(e));
}

static int parse_port(n3_port *restrict out, const char *restrict string) {
    char *endptr;
    errno = 0;
    long l = strtol(string, &endptr, 10);
    if(errno)
        return errno;
    if(*endptr)
        return EINVAL;
    if(l < 0 || l > UINT16_MAX)
        return ERANGE;
    *out = (n3_port)l;
    return 0;
}

static void on_unlink(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote,
    _Bool timeout,
    void *data
) {
    struct unlink *restrict unlink = data;

    unlink->unlinked = 1;
    unlink->timeout = timeout;
}

static void init(
    struct state *restrict state,
    const struct args *restrict args
) {
    state->args = args;

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    if(flags == -1)
        b3_fatal("Error getting stdin fcntl flags: %s", strerror(errno));
    flags |= O_NONBLOCK;
    if(fcntl(STDIN_FILENO, F_SETFL, flags) == -1)
        b3_fatal("Error setting stdin fcntl flags: %s", strerror(errno));

    setbuf(stdout, NULL);

    n3_port port;
    int error = parse_port(&port, args->port);
    if(error)
        b3_fatal("Error parsing port '%s': %s", args->port, strerror(error));

    n3_init(args->verbosity, stderr);

    n3_host host;
    if(args->hostname)
        n3_init_host(&host, args->hostname, port);
    else
        n3_init_host_any_local(&host, port);

    n3_terminal_options options = {.remote_unlink_callback = on_unlink};
    if(args->listen) {
        state->terminal = n3_new_terminal(&host, NULL, &options);
    }
    else {
        state->remote_host = host;

        n3_link *server_link = n3_new_link(&host, &options);
        state->terminal = n3_get_terminal(server_link);
        n3_free_link(server_link);
    }
}

static void quit(struct state *restrict state) {
    memset(&state->remote_host, 0, sizeof(state->remote_host));
    n3_free_terminal(state->terminal);
    state->terminal = NULL;

    n3_quit();
}

static _Bool keep_going(
    struct state *restrict state,
    struct unlink *restrict unlink
) {
    if(state->args->listen || !unlink->unlinked)
        return 1;

    if(state->exit_status == 0 && unlink->timeout)
        state->exit_status = 1;
    return 0;
}

static n3_buffer *add_identity(
    n3_buffer *restrict data,
    const n3_host *restrict host
) {
    size_t data_cap = n3_get_buffer_cap(data);

    size_t identity_size = N3_ADDRESS_SIZE + 10; // "<address:port>: ".
    n3_buffer *buffer = n3_new_buffer(identity_size + data_cap, NULL);

    char address[N3_ADDRESS_SIZE];
    n3_get_host_address(host, address, sizeof(address));
    n3_port port = n3_get_host_port(host);

    char *buf = n3_get_buffer(buffer);
    int identity_cap = snprintf(
        buf,
        identity_size,
        "<%s|%"PRIu16">: ",
        address,
        port
    );
    memcpy(buf + identity_cap, n3_get_buffer(data), data_cap);
    n3_set_buffer_cap(buffer, identity_cap + data_cap);

    n3_free_buffer(data);
    return buffer;
}

static void resend(
    n3_terminal *restrict terminal,
    const n3_host *restrict remote,
    void *data
) {
    struct rebroadcast *restrict rebroadcast = data;

    if(n3_compare_hosts(remote, rebroadcast->from_host))
        n3_send_to(terminal, 0, rebroadcast->buffer, remote);
}

static _Bool receive_data(struct state *restrict state) {
    struct unlink unlink = UNLINK_INIT;
    n3_host remote;

    for(
        n3_buffer *data;
        (data = n3_receive(state->terminal, NULL, &remote, NULL, &unlink))
                != NULL;
   ) {
        if(state->args->listen && state->args->identify)
            data = add_identity(data, &remote);
        if(state->args->listen && state->args->broadcast) {
            n3_for_each_link(state->terminal, resend, &(struct rebroadcast) {
                .from_host = &remote,
                .buffer = data
            });
        }

        fwrite(n3_get_buffer(data), 1, n3_get_buffer_cap(data), stdout);
        n3_free_buffer(data);
    }

    return keep_going(state, &unlink);
}

static _Bool send_input(struct state *restrict state) {
    while(1) {
        // Note: we don't do any buffering of our own, which means we rely on
        // terminal line mode to give us one line at a time.  For stdin not
        // connected to a terminal, we just push data out as soon as we get it,
        // no matter how small.  This could be inefficient, but it's easy.
        char buf[N3_SAFE_BUFFER_SIZE];
        ssize_t read_size = read(STDIN_FILENO, buf, sizeof(buf));
        if(read_size == -1) {
            if(errno == EINTR)
                continue;
            if(errno == EAGAIN)
                break;
            b3_fatal("Error reading from stdin: %s", strerror(errno));
        }
        if(read_size == 0)
            return 0;

        n3_buffer *buffer = n3_build_buffer(buf, read_size, NULL);
        if(state->args->listen && state->args->identify) {
            n3_host local;
            n3_get_host(state->terminal, &local);
            buffer = add_identity(buffer, &local);
        }

        if(state->args->listen)
            n3_broadcast(state->terminal, 0, buffer);
        else
            n3_send_to(state->terminal, 0, buffer, &state->remote_host);
        n3_free_buffer(buffer);
    }

    return 1;
}

static _Bool pump(struct state *restrict state) {
    struct pollfd pollfds[] = {
        // TODO: only poll/read from stdin once a connection is established.
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = n3_get_fd(state->terminal), .events = POLLIN },
        // TODO: also use signalfd, poll for SIGINT/SIGTERM, handle gracefully.
    };

    if(poll(pollfds, B3_STATIC_ARRAY_COUNT(pollfds), UPDATE_TIMEOUT_MS) == -1
            && errno != EINTR)
        b3_fatal("Error polling: %s", strerror(errno));

    if(pollfds[1].revents & POLLIN) {
        if(!receive_data(state))
            return 0;
    }
    if(pollfds[0].revents & POLLIN) {
        if(!send_input(state))
            return 0;
    }

    struct unlink unlink = UNLINK_INIT;
    n3_update(state->terminal, &unlink);

    return keep_going(state, &unlink);
}

int main(int argc, char *argv[]) {
    struct args args = ARGS_INIT_DEFAULT;
    parse_args(&args, argc, argv);

    struct state state = STATE_INIT;
    init(&state, &args);

    while(pump(&state))
        continue;

    quit(&state);
    return state.exit_status;
}
