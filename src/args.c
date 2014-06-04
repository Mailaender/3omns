#include "3omns.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <argp.h>


#define LOCAL_RESOURCES "./res"
#define INSTALLED_RESOURCES DATADIR


const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static const char *const local_check_file
        = LOCAL_RESOURCES"/"DEFAULT_GAME"/init.lua";


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

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct args *restrict args = state->input;

    switch(key) {
    case 'r':
        args->resources = arg;
        break;
    case 'g':
        args->game = arg;
        break;
    case 'd':
        args->debug = 1;
        break;
    case 'n':
        args->debug_network = 1;
        break;
    case 'c':
        args->client = 1;
        args->hostname = arg;
        break;
    case 's':
        args->serve = 1;
        args->hostname = arg;
        break;
    case 'p':
        {
            int e = parse_port(&args->port, arg);
            if(e)
                b3_fatal("Error parsing port '%s': %s", arg, strerror(e));
            break;
        }
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static const char *find_default_resources(void) {
    if(!access(local_check_file, R_OK))
        return LOCAL_RESOURCES;
    return INSTALLED_RESOURCES;
}

void parse_args(struct args *restrict args, int argc, char *argv[]) {
    args->resources = find_default_resources();

    // TODO: I guess argp handles gettext-ing these itself?
    static const char doc[]
        = {"Old-school arcade-style tile-based bomb-dropping deathmatch jam"};
    static struct argp_option options[] = {
        {"resources", 'r', "RES", 0, "Location of game resources (default: "
                "'"LOCAL_RESOURCES"' if valid, or "
                "'"INSTALLED_RESOURCES"')"},
        // TODO: let this be controlled from in-game.
        {"game", 'g', "MOD", 0, "Run Lua game code from MOD (default: "
                "'"DEFAULT_GAME"')"},
        {NULL, 0, NULL, 0, "Debug options:", 1},
        {"debug", 'd', NULL, 0, "Run in debug mode", 1},
        {"debug-network", 'n', NULL, 0, "Print network communication", 1},
        // TODO: let these be controlled from in-game.
        {NULL, 0, NULL, 0, "Network play options:", 2},
        {"connect", 'c', "SERVER", 0, "Connect to network host", 2},
        {"serve", 's', "LISTEN", OPTION_ARG_OPTIONAL, "Host network game on "
                "the given address (default: *)", 2},
        {"port", 'p', "PORT", 0, "Port for serving or connecting (default: "
                B3_STRINGIFY(DEFAULT_PORT)")", 2},
        {0}
    };
    static struct argp argp = {options, parse_opt, NULL, doc};

    error_t e = argp_parse(&argp, argc, argv, 0, NULL, args);
    if(e)
        b3_fatal("Error parsing arguments: %s", strerror(e));
}