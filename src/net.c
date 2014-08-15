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
#include "l3/l3.h"
#include "n3/n3.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define PROTOCOL_VERSION '2'

struct notify_entity_data {
    _Bool dirty_only;
    n3_buffer *buffer;
    const n3_host *host;
};


static n3_terminal *terminal = NULL;

static int sent_packets = 0;
static int received_packets = 0;


static const char *host_to_string(const n3_host *restrict host) {
    static char string[N3_ADDRESS_SIZE + 10]; // 10 for "UDP |12345".
    char address[N3_ADDRESS_SIZE] = {""};
    n3_get_host_address(host, address, sizeof(address));
    n3_port port = n3_get_host_port(host);
    snprintf(string, sizeof(string), "UDP %s|%"PRIu16, address, port);
    return string;
}

static void debug_network_print(
    n3_buffer *restrict buffer,
    size_t size,
    const char *restrict format,
    ...
) {
    if(!args.debug_network)
        return;

    va_list args;
    va_start(args, format);

    vfprintf(DEBUG_NETWORK_FILE, format, args);
    fwrite(n3_get_buffer(buffer), 1, size, DEBUG_NETWORK_FILE);
    fputc('\n', DEBUG_NETWORK_FILE);

    va_end(args);
}

static void append_buffer(
    n3_buffer *restrict buffer,
    const char *restrict format,
    ...
) {
    va_list args;
    va_start(args, format);

    size_t size = n3_get_buffer_size(buffer);
    size_t cap = n3_get_buffer_cap(buffer);
    int printed = vsnprintf(
        (char *)n3_get_buffer(buffer) + cap,
        size - cap,
        format,
        args
    );
    if(cap + printed > size) // Ignore terminating null.
        b3_fatal("Send buffer too small");
    n3_set_buffer_cap(buffer, cap + printed);

    va_end(args);
}

// Assume buffer is null-terminated.  This overloads the buffer cap to keep
// track of where we've scanned to, which is a bit odd, but works and is easy.
#define scan_buffer(buffer, assignments, format, ...) do { \
    size_t cap = n3_get_buffer_cap(buffer); \
    int consumed = 0; \
    scan_buffer_( \
        (buffer), \
        cap, \
        (assignments), \
        format, \
        format "%n", \
        __VA_ARGS__, \
        &consumed \
    ); \
    n3_set_buffer_cap((buffer), cap + consumed); \
} while(0)

static void scan_buffer_(
    n3_buffer *restrict buffer,
    size_t cap,
    int assignments,
    const char *restrict parse_format,
    const char *restrict format,
    ...
) {
    va_list args;
    va_start(args, format);

    int assigned = vsscanf((char *)n3_get_buffer(buffer) + cap, format, args);
    if(assigned < assignments) {
        b3_fatal(
            "Error parsing received message; expected scanf-format: %s",
            parse_format
        );
    }

    va_end(args);
}

static void send_notification(
    n3_buffer *restrict buffer,
    const n3_host *restrict host
) {
    if(!args.client && !args.serve)
        return;

    size_t buffer_size = n3_get_buffer_cap(buffer);
    if(host) {
        debug_network_print(
            buffer,
            buffer_size,
            "Sent to %s: ",
            host_to_string(host)
        );
        n3_send_to(terminal, 0, buffer, host);
    }
    else {
        debug_network_print(buffer, buffer_size, "Broadcast: ");
        n3_broadcast(terminal, 0, buffer);
    }
    sent_packets++;
}

static n3_buffer *receive_notification(
    struct round *restrict round,
    n3_host *restrict host
) {
    if(!args.client && !args.serve)
        return NULL;

    n3_host host_;
    n3_host *h = (host ? host : &host_);

    n3_buffer *buffer = n3_receive(terminal, h, round, round);
    if(buffer) {
        received_packets++;
        debug_network_print(
            buffer,
            n3_get_buffer_size(buffer) - 1,
            "Received from %s: ",
            host_to_string(h)
        );
    }
    return buffer;
}

static n3_buffer *new_buffer(
    size_t size,
    const n3_allocator *restrict allocator
) {
    // Add space for the terminating NUL, and start cap at 0 (which we use to
    // track what's been printed/scanned thus far).
    n3_buffer *buffer = n3_new_buffer(size + 1, allocator);
    n3_set_buffer_cap(buffer, 0);

    // Add NUL just so it's always a printable string.
    char *b = n3_get_buffer(buffer);
    b[0] = '\0';

    return buffer;
}

static void notify_paused_state(
    const struct round *restrict round,
    const n3_host *restrict host
) {
    n3_buffer *buffer = new_buffer(2, NULL);
    append_buffer(buffer, "p%c", (round->paused ? '1' : '0'));
    send_notification(buffer, host);

    n3_free_buffer(buffer);
}

static void process_paused_state(
    struct round *restrict round,
    n3_buffer *restrict buffer
) {
    char p = '0';
    scan_buffer(buffer, 1, "p%c", &p);

    _Bool paused = (p == '0' ? 0 : 1);
    if(round->paused != paused) {
        round->paused = paused;
        if(args.serve)
            notify_paused_state(round, NULL);
    }

    n3_free_buffer(buffer);
}

void notify_paused_changed(const struct round *restrict round) {
    notify_paused_state(round, NULL);
}

void notify_input(const struct round *restrict round, b3_input input) {
    if(!args.client)
        return;

    int player = B3_INPUT_PLAYER(input);
    int button;
    if(input == B3_INPUT_UP(player)) button = 'u';
    else if(input == B3_INPUT_DOWN(player)) button = 'd';
    else if(input == B3_INPUT_LEFT(player)) button = 'l';
    else if(input == B3_INPUT_RIGHT(player)) button = 'r';
    else button = 'f';

    n3_buffer *buffer = new_buffer(3, NULL);
    append_buffer(buffer, "i%c%c", player + '0', button);
    send_notification(buffer, NULL);

    n3_free_buffer(buffer);
}

static void process_input(
    struct round *restrict round,
    n3_buffer *restrict buffer
) {
    char p = '0';
    char b = '0';
    scan_buffer(buffer, 2, "i%c%c", &p, &b);

    int player = p - '0';
    if(player < 0 || player > 3)
        b3_fatal("Received invalid input event");

    // TODO: map the remote player to an appropriate local one.

    b3_input input;
    if(b == 'u') input = B3_INPUT_UP(player);
    else if(b == 'd') input = B3_INPUT_DOWN(player);
    else if(b == 'l') input = B3_INPUT_LEFT(player);
    else if(b == 'r') input = B3_INPUT_RIGHT(player);
    else input = B3_INPUT_FIRE(player);

    if(!round->paused && args.serve)
        l3_input(&round->level, input);

    n3_free_buffer(buffer);
}

static void notify_map(
    const struct round *restrict round,
    const n3_host *restrict host
) {
    n3_buffer *buffer = new_buffer(N3_SAFE_BUFFER_SIZE, NULL);

    append_buffer(
        buffer,
        "m%Xx%X-%X",
        round->map_size.width,
        round->map_size.height,
        b3_get_entity_pool_size(round->level.entities)
    );

    for(int i = 0; i < L3_DUDE_COUNT; i++)
        append_buffer(buffer, "$%X", round->level.dude_ids[i]);

    b3_tile run_tile = 0;
    int run_count = 0;
    for(int y = 0; y < round->map_size.height; y++) {
        for(int x = 0; x < round->map_size.width; x++) {
            b3_tile tile = b3_get_map_tile(round->level.map, &(b3_pos){x, y});

            if(tile == run_tile)
                run_count++;
            else {
                if(run_count > 0)
                    append_buffer(buffer, "*%X:%c", run_count, (int)run_tile);
                run_tile = tile;
                run_count = 1;
            }
        }
    }
    append_buffer(buffer, "*%X:%c", run_count, (int)run_tile);

    send_notification(buffer, host);

    n3_free_buffer(buffer);
}

static void process_map(
    struct round *restrict round,
    n3_buffer *restrict buffer
) {
    if(round->initialized)
        return;

    char *buf = n3_get_buffer(buffer);

    int width = 0;
    int height = 0;
    int max_entities = 0;
    scan_buffer(buffer, 3, "m%Xx%X-%X", &width, &height, &max_entities);

    // TODO: these maxima should be a bit more rigorously defined.
    if(width <= 0 || height <= 0 || max_entities <= 0
            || width > 10000 || height > 10000 || max_entities > 10000)
        b3_fatal("Received invalid map data");

    l3_init_level(
        &round->level,
        args.client,
        &(b3_size){width, height},
        max_entities
    );
    round->map_size = b3_get_map_size(round->level.map);
    round->tile_size = b3_get_map_tile_size(&round->map_size, &game_size);

    for(int i = 0; i < L3_DUDE_COUNT; i++)
        scan_buffer(buffer, 1, "$%X", &round->level.dude_ids[i]);

    b3_pos map_pos = {0, 0};
    while(buf[n3_get_buffer_cap(buffer)] == '*') {
        b3_tile run_tile = 0;
        int run_count = 0;
        scan_buffer(buffer, 2, "*%X:%c", &run_count, &run_tile);

        for(int i = 0; i < run_count; i++) {
            if(map_pos.y >= height)
                b3_fatal("Received too much data for map");

            b3_set_map_tile(round->level.map, &map_pos, run_tile);
            if(++map_pos.x >= width) {
                map_pos.x = 0;
                map_pos.y++;
            }
        }
    }

    l3_set_sync_level(&round->level);

    round->initialized = 1;

    n3_free_buffer(buffer);
}

static void notify_entity(b3_entity *restrict entity, void *callback_data) {
    struct notify_entity_data *d = callback_data;

    if(d->dirty_only && !b3_get_entity_dirty(entity))
        return;

    size_t serial_len = 0;
    char *serial = l3_serialize_entity(entity, &serial_len);

    if(d->buffer) {
        size_t cap = n3_get_buffer_cap(d->buffer);
        size_t size = n3_get_buffer_size(d->buffer);

        // TODO: this approximation should be more exact.
        if(cap + serial_len + 50 > size) {
            send_notification(d->buffer, d->host);
            n3_free_buffer(d->buffer);
            d->buffer = NULL;
        }
    }
    if(!d->buffer)
        d->buffer = new_buffer(N3_SAFE_BUFFER_SIZE, NULL);

    if(!n3_get_buffer_cap(d->buffer))
        append_buffer(d->buffer, "e");

    // TODO: only transmit the delta, instead of the full line.
    b3_pos entity_pos = b3_get_entity_pos(entity);
    append_buffer(
        d->buffer,
        "#%X:%X,%X-%X|%zX:",
        b3_get_entity_id(entity),
        entity_pos.x,
        entity_pos.y,
        b3_get_entity_life(entity),
        serial_len
    );
    if(serial)
        append_buffer(d->buffer, "%*s", (int)serial_len, serial);

    if(d->dirty_only)
        b3_set_entity_dirty(entity, 0);
    b3_free(serial, 0);
}

static void notify_entities(
    _Bool dirty_only,
    const struct round *restrict round,
    const n3_host *restrict host
) {
    struct notify_entity_data d = {dirty_only, NULL, host};

    b3_for_each_entity(round->level.entities, notify_entity, &d);

    if(d.buffer && n3_get_buffer_cap(d.buffer))
        send_notification(d.buffer, host);

    n3_free_buffer(d.buffer);
}

static void process_entities(
    struct round *restrict round,
    n3_buffer *restrict buffer
) {
    char *buf = n3_get_buffer(buffer);

    n3_set_buffer_cap(buffer, 1); // Skip initial 'e'.

    while(buf[n3_get_buffer_cap(buffer)] == '#') {
        b3_entity_id id = 0;
        int x = 0;
        int y = 0;
        int life = 0;
        size_t serial_len = 0;
        scan_buffer(
            buffer,
            5,
            "#%X:%X,%X-%X|%zX:",
            &id,
            &x,
            &y,
            &life,
            &serial_len
        );
        size_t cap = n3_get_buffer_cap(buffer);
        const char *serial = &buf[cap];
        n3_set_buffer_cap(buffer, cap + serial_len);

        if(x < 0 || y < 0 || life < 0 || serial_len > 10000)
            b3_fatal("Received invalid entity data");

        l3_sync_entity(id, &(b3_pos){x, y}, life, serial, serial_len);
    }

    n3_free_buffer(buffer);
}

static void notify_deleted_entities(const struct round *restrict round) {
    int count = 0;
    const b3_entity_id *ids = b3_get_released_ids(
        round->level.entities,
        &count
    );
    if(!count)
        return;
    // Note that it's possible for an entity to have been created and destroyed
    // before we ever sent out a notification about its existence.  We still
    // get that id from b3_get_released_ids(), so in that case, it'll look to
    // the client like we're notifying about a nonexistent entity being
    // deleted.  It shouldn't matter, it's just an odd case.

    n3_buffer *buffer = new_buffer(N3_SAFE_BUFFER_SIZE, NULL);

    append_buffer(buffer, "d");
    for(int i = 0; i < count; i++)
        append_buffer(buffer, "#%X", ids[i]);

    send_notification(buffer, NULL);

    n3_free_buffer(buffer);
    b3_clear_released_ids(round->level.entities);
}

static void process_deleted_entities(
    struct round *restrict round,
    n3_buffer *restrict buffer
) {
    char *buf = n3_get_buffer(buffer);

    n3_set_buffer_cap(buffer, 1); // Skip initial 'd'.

    // size/2 should be a safe maximum number of ids, since the minimum size of
    // an id is one digit, and each one is separated by one '#'.
    b3_entity_id ids[n3_get_buffer_size(buffer) / 2];
    int id_count = 0;

    while(buf[n3_get_buffer_cap(buffer)] == '#') {
        b3_entity_id id = 0;
        scan_buffer(buffer, 1, "#%X", &id);

        ids[id_count++] = id;
    }

    l3_sync_deleted(ids, id_count);

    for(int i = 0; i < id_count; i++) {
        b3_entity *entity = b3_get_entity(round->level.entities, ids[i]);
        b3_release_entity(entity);
    }

    n3_free_buffer(buffer);
}

void notify_updates(const struct round *restrict round) {
    if(!args.serve)
        return;

    notify_deleted_entities(round);
    notify_entities(1, round, NULL);
}

static void notify_connect(void) {
    n3_buffer *buffer = new_buffer(2, NULL);
    append_buffer(buffer, "c%c", PROTOCOL_VERSION);
    send_notification(buffer, NULL);

    n3_free_buffer(buffer);
}

static void process_connect(
    struct round *restrict round,
    n3_buffer *restrict buffer,
    const n3_host *restrict host
) {
    char v = '0';
    scan_buffer(buffer, 1, "c%c", &v);

    // TODO: actively disconnect or tell the client they have an incompatible
    // version, instead of just ignoring them.
    if(v == PROTOCOL_VERSION) {
        notify_paused_state(round, host);
        notify_map(round, host);
        notify_entities(0, round, host);
    }

    n3_free_buffer(buffer);
}

void process_notifications(struct round *restrict round) {
    n3_host host;
    for(
        n3_buffer *buffer;
        (buffer = receive_notification(round, &host)) > 0;
    ) {
        switch(((char *)n3_get_buffer(buffer))[0]) {
        case 'c': process_connect(round, buffer, &host); break;
        case 'p': process_paused_state(round, buffer); break;
        case 'i': process_input(round, buffer); break;
        case 'm': process_map(round, buffer); break;
        case 'e': process_entities(round, buffer); break;
        case 'd': process_deleted_entities(round, buffer); break;
        default: b3_fatal("Received unknown notification");
        }
    }
}

void get_net_debug_stats(struct debug_stats *restrict debug_stats) {
    debug_stats->sent_packets = sent_packets;
    debug_stats->received_packets = received_packets;
}

static _Bool filter_new_link(
    n3_terminal *restrict terminal,
    const n3_host *restrict host,
    void *data
) {
    // const struct round *restrict round = data;

    DEBUG_PRINT("%s connected\n", host_to_string(host));
    return 1;
}

static void handle_remote_unlink(
    n3_terminal *restrict terminal,
    const n3_host *restrict host,
    _Bool timeout,
    void *data
) {
    // const struct round *restrict round = data;

    DEBUG_PRINT("%s disconnected\n", host_to_string(host));
}

static n3_buffer *build_receive_buffer(
    void *restrict buf,
    size_t size,
    const n3_allocator *restrict allocator
) {
    n3_buffer *buffer = new_buffer(size, allocator);

    // Add terminating NUL so we can safely scan_buffer().
    char *b = n3_get_buffer(buffer);
    memcpy(b, buf, size);
    b[size] = '\0';

    return buffer;
}

void init_net(void) {
    if(!args.client && !args.serve)
        return;

    n3_init(args.protocol_verbosity, DEBUG_FILE);

    n3_host host;
    if(args.client || (args.serve && args.hostname))
        n3_init_host(&host, args.hostname, args.port);
    else if(args.serve)
        n3_init_host_any_local(&host, args.port);

    n3_terminal_options options = N3_TERMINAL_OPTIONS_INIT;
    options.build_receive_buffer = build_receive_buffer;
    options.remote_unlink_callback = handle_remote_unlink;

    if(args.client) {
        n3_link *server_link = n3_new_link(&host, &options);
        terminal = n3_get_terminal(server_link);
        n3_free_link(server_link);

        DEBUG_PRINT("Connecting to %s\n", host_to_string(&host));
        notify_connect();
    }
    else if(args.serve) {
        terminal = n3_new_terminal(&host, filter_new_link, &options);

        DEBUG_PRINT("Listening at %s\n", host_to_string(&host));
    }
}

void quit_net(void) {
    n3_free_terminal(terminal);
    terminal = NULL;

    n3_quit();
}

void update_net(struct round *restrict round) {
    if(!args.client && !args.serve)
        return;

    n3_update(terminal, round);
}
