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

#include <argp.h>
#include <errno.h>
#include <fcntl.h>
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
};
#define STATE_INIT {.terminal = NULL}


const char *argp_program_version = "n3c 0.0+git";
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

static int parse_port(n3_port *out, const char *string) {
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

    if(args->listen) {
        state->terminal = n3_new_terminal(&host, NULL, NULL);
    }
    else {
        state->remote_host = host;

        n3_link *server_link = n3_new_link(&host, NULL);
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

static void receive_data(struct state *restrict state) {
    for(
        n3_buffer *data;
        (data = n3_receive(state->terminal, NULL, NULL, NULL, NULL)) != NULL;
    ) {
        // TODO: check for connection ending, close if client.
        // TODO: identify?  Re-broadcast?
        fwrite(n3_get_buffer(data), 1, n3_get_buffer_cap(data), stdout);
        n3_free_buffer(data);
    }
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
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = n3_get_fd(state->terminal), .events = POLLIN },
        // TODO: also use signalfd, poll for SIGINT/SIGTERM, handle gracefully.
    };

    if(poll(pollfds, B3_STATIC_ARRAY_COUNT(pollfds), UPDATE_TIMEOUT_MS) == -1
            && errno != EINTR)
        b3_fatal("Error polling: %s", strerror(errno));

    if(pollfds[1].revents & POLLIN)
        receive_data(state);
    if(pollfds[0].revents & POLLIN) {
        if(!send_input(state))
            return 0;
    }

    n3_update(state->terminal, NULL);

    return 1;
}

int main(int argc, char *argv[]) {
    struct args args = ARGS_INIT_DEFAULT;
    parse_args(&args, argc, argv);

    struct state state = STATE_INIT;
    init(&state, &args);

    while(pump(&state))
        continue;

    quit(&state);
    return 0;
}
