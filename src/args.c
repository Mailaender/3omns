/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
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

#include "3omns.h"
#include "b3/b3.h"

#include <argp.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define LOCAL_RESOURCES "./res"
#define INSTALLED_RESOURCES DATADIR

#define LOCAL_CHECK_FILE LOCAL_RESOURCES"/"DEFAULT_GAME"/init.lua"


const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = "<"PACKAGE_BUGREPORT">";


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
    case 'd':
        args->debug = 1;
        break;
    case 'n':
        args->debug_network = 1;
        break;
    case 'R':
        puts(INSTALLED_RESOURCES);
        exit(0);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static const char *find_default_resources(void) {
    if(!access(LOCAL_CHECK_FILE, R_OK))
        return LOCAL_RESOURCES;
    return INSTALLED_RESOURCES;
}

void parse_args(struct args *restrict args, int argc, char *argv[]) {
    args->resources = find_default_resources();

    // TODO: I guess argp handles gettext-ing these itself?
    const char *const doc
            = "Old-school arcade-style tile-based bomb-dropping deathmatch jam"
            "\vSee <"PACKAGE_URL"> for more information.";

    struct argp_option options[] = {
        {"resources", 'r', "RES", 0, "Location of game resources (default: "
                "'"LOCAL_RESOURCES"' if valid, or "
                "'"INSTALLED_RESOURCES"')"},
        // TODO: let this be controlled from in-game.
        {"game", 'g', "MOD", 0, "Run Lua game code from MOD (default: "
                "'"DEFAULT_GAME"')"},
        // TODO: let these be controlled from in-game.
        {NULL, 0, NULL, 0, "Network play options:", 1},
        {"connect", 'c', "SERVER", 0, "Connect to network host", 1},
        {"serve", 's', "LISTEN", OPTION_ARG_OPTIONAL, "Host network game on "
                "the given address (default: *)", 1},
        {"port", 'p', "PORT", 0, "Port for serving or connecting (default: "
                B3_STRINGIFY(DEFAULT_PORT)")", 1},
        {NULL, 0, NULL, 0, "Debug options:", 2},
        {"debug", 'd', NULL, 0, "Run in debug mode", 2},
        {"debug-network", 'n', NULL, 0, "Print network communication", 2},
        {NULL, 0, NULL, 0, "Informational options:", 3},
        {"default-resources", 'R', NULL, 0, "Print default resources path "
                "('"INSTALLED_RESOURCES"') and exit", 3},
        {0}
    };
    struct argp argp = {options, parse_opt, NULL, doc};

    error_t e = argp_parse(&argp, argc, argv, 0, NULL, args);
    if(e)
        b3_fatal("Error parsing arguments: %s", strerror(e));
}
