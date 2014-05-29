#include "3omns.h"
#include "b3/b3.h"
#include "l3/l3.h"
#include "n3/n3.h"

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>


struct notify_entity_data {
    uint8_t *buf;
    size_t size;
    int *pos;

    const n3_host *host;
};


static n3_server *server = NULL;
static n3_client *client = NULL;


static const char *host_to_string(const n3_host *restrict host) {
    static char string[N3_ADDRESS_SIZE + 10]; // 10 for "UDP |12345".
    char address[N3_ADDRESS_SIZE] = {""};
    n3_get_host_address(host, address, sizeof(address));
    n3_port port = n3_get_host_port(host);
    snprintf(string, sizeof(string), "UDP %s|%"PRIu16, address, port);
    return string;
}

static void debug_network_print(
    const uint8_t *restrict buf,
    size_t size,
    const char *restrict format,
    ...
) {
    if(!args.debug_network)
        return;

    va_list args;
    va_start(args, format);

    vfprintf(DEBUG_NETWORK_FILE, format, args);
    fwrite(buf, 1, size, DEBUG_NETWORK_FILE);
    fputc('\n', DEBUG_NETWORK_FILE);

    va_end(args);
}

static void print_buffer(
    uint8_t *restrict buf,
    size_t size,
    int *restrict pos,
    const char *restrict format,
    ...
) {
    va_list args;
    va_start(args, format);

    *pos += vsnprintf((char *)buf + *pos, size - *pos, format, args);
    if((size_t)*pos > size) // Ignore terminating null.
        b3_fatal("Send buffer too small");

    va_end(args);
}

// Assume buf is null-terminated.
#define scan_buffer(buf, pos, assignments, format, ...) do { \
    int consumed = 0; \
    scan_buffer_( \
        (buf), \
        *(pos), \
        (assignments), \
        format, \
        format "%n", \
        __VA_ARGS__, \
        &consumed \
    ); \
    *(pos) += consumed; \
} while(0)

static void scan_buffer_(
    const uint8_t *restrict buf,
    int pos,
    int assignments,
    const char *restrict parse_format,
    const char *restrict format,
    ...
) {
    va_list args;
    va_start(args, format);

    int assigned = vsscanf((char *)buf + pos, format, args);
    if(assigned < assignments) {
        b3_fatal(
            "Error parsing received message; expected scanf-format: %s",
            parse_format
        );
    }

    va_end(args);
}

static void send_notification(
    const uint8_t *restrict buf,
    size_t size,
    const n3_host *restrict host
) {
    if(args.client) {
        debug_network_print(buf, size, "Send: ");
        n3_client_send(client, buf, size);
    }
    else if(args.serve) {
        if(host) {
            debug_network_print(
                buf,
                size,
                "Send to %s: ",
                host_to_string(host)
            );
            n3_send_to(server, buf, size, host);
        }
        else {
            debug_network_print(buf, size, "Broadcast: ");
            n3_broadcast(server, buf, size);
        }
    }
}

static size_t receive_notification(
    struct round *restrict round,
    uint8_t *restrict buf,
    size_t size,
    n3_host *restrict host
) {
    size_t received = 0;
    if(args.client) {
        received = n3_client_receive(client, buf, size);
        if(received)
            debug_network_print(buf, received, "Received: ");
    }
    else if(args.serve) {
        n3_host received_host;
        n3_host *restrict r_host = (host ? host : &received_host);
        received = n3_server_receive(server, buf, size, r_host, round);
        if(received) {
            debug_network_print(
                buf,
                received,
                "Received from %s: ",
                host_to_string(r_host)
            );
        }
    }
    return received;
}

static void notify_paused_state(
    const struct round *restrict round,
    const n3_host *restrict host
) {
    uint8_t buf[] = {'p', (round->paused ? '1' : '0')};
    send_notification(buf, sizeof(buf), host);
}

static void process_paused_state(
    struct round *restrict round,
    const uint8_t *restrict buf,
    size_t size
) {
    _Bool p = (buf[1] == '0' ? 0 : 1);
    if(round->paused != p) {
        round->paused = p;
        if(args.serve)
            notify_paused_state(round, NULL);
    }
}

void notify_paused_changed(const struct round *restrict round) {
    notify_paused_state(round, NULL);
}

void notify_input(const struct round *restrict round, b3_input input) {
    int player = B3_INPUT_PLAYER(input);
    int button;
    if(input == B3_INPUT_UP(player)) button = 'u';
    else if(input == B3_INPUT_DOWN(player)) button = 'd';
    else if(input == B3_INPUT_LEFT(player)) button = 'l';
    else if(input == B3_INPUT_RIGHT(player)) button = 'r';
    else button = 'f';

    uint8_t buf[] = {'i', player + '0', button};
    send_notification(buf, sizeof(buf), NULL);
}

static void process_input(
    struct round *restrict round,
    const uint8_t *restrict buf,
    size_t size
) {
    int player = buf[1] - '0';
    if(size < 3 || player < 0 || player > 3)
        b3_fatal("Received invalid input event");

    // TODO: map the remote player to an appropriate local one.

    b3_input input;
    if(buf[2] == 'u') input = B3_INPUT_UP(player);
    else if(buf[2] == 'd') input = B3_INPUT_DOWN(player);
    else if(buf[2] == 'l') input = B3_INPUT_LEFT(player);
    else if(buf[2] == 'r') input = B3_INPUT_RIGHT(player);
    else input = B3_INPUT_FIRE(player);

    l3_input(&round->level, input);
}

static void notify_map(
    const struct round *restrict round,
    const n3_host *restrict host
) {
    uint8_t buf[N3_SAFE_BUFFER_SIZE];
    int pos = 0;

#define PRINT_MAP(...) print_buffer(buf, sizeof(buf), &pos, __VA_ARGS__)
    PRINT_MAP(
        "m%Xx%X-%X/",
        round->map_size.width,
        round->map_size.height,
        b3_get_entity_pool_size(round->level.entities)
    );

    for(int i = 0; i < L3_DUDE_COUNT - 1; i++)
        PRINT_MAP("%X,", round->level.dude_ids[i]);
    PRINT_MAP("%X|", round->level.dude_ids[L3_DUDE_COUNT - 1]);

    b3_tile run_tile = 0;
    int run_count = 0;
    for(int y = 0; y < round->map_size.height; y++) {
        for(int x = 0; x < round->map_size.width; x++) {
            b3_tile tile = b3_get_map_tile(round->level.map, &(b3_pos){x, y});

            if(tile == run_tile)
                run_count++;
            else {
                if(run_count > 0)
                    PRINT_MAP("%X:%c;", run_count, (int)run_tile);
                run_tile = tile;
                run_count = 1;
            }
        }
    }
    PRINT_MAP("%X:%c", run_count, (int)run_tile);
#undef PRINT_MAP

    send_notification(buf, (size_t)pos, host);
}

static void process_map(
    struct round *restrict round,
    const uint8_t *restrict buf,
    size_t size
) {
    if(round->initialized)
        return;

    int pos = 0;

#define SCAN_MAP(...) scan_buffer(buf, &pos, __VA_ARGS__)
    int width = 0;
    int height = 0;
    int max_entities = 0;
    SCAN_MAP(3, "m%Xx%X-%X/", &width, &height, &max_entities);

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

    for(int i = 0; i < L3_DUDE_COUNT - 1; i++)
        SCAN_MAP(1, "%X,", &round->level.dude_ids[i]);
    SCAN_MAP(1, "%X|", &round->level.dude_ids[L3_DUDE_COUNT - 1]);

    b3_pos map_pos = {0, 0};
    do {
        b3_tile run_tile = 0;
        int run_count = 0;
        SCAN_MAP(2, "%X:%c", &run_count, &run_tile);

        for(int i = 0; i < run_count; i++) {
            if(map_pos.y >= height)
                b3_fatal("Received too much data for map");

            b3_set_map_tile(round->level.map, &map_pos, run_tile);
            if(++map_pos.x >= width) {
                map_pos.x = 0;
                map_pos.y++;
            }
        }
    } while(buf[pos++] == ';');
#undef SCAN_MAP

    l3_set_sync_level(&round->level);

    round->initialized = 1;
}

static void notify_entity(b3_entity *restrict entity, void *callback_data) {
    struct notify_entity_data *d = callback_data;

    size_t serial_len = 0;
    char *serial = l3_serialize_entity(entity, &serial_len);

    // TODO: this approximation should be more exact.
    if((size_t)*d->pos + serial_len + 50 > d->size) {
        send_notification(d->buf, (size_t)*d->pos, d->host);
        *d->pos = 0;
    }

#define PRINT_ENTITY(...) print_buffer(d->buf, d->size, d->pos, __VA_ARGS__)
    if(!*d->pos)
        PRINT_ENTITY("e");

    b3_pos entity_pos = b3_get_entity_pos(entity);
    PRINT_ENTITY(
        "#%X:%X,%X-%X|%zX:",
        b3_get_entity_id(entity),
        entity_pos.x,
        entity_pos.y,
        b3_get_entity_life(entity),
        serial_len
    );
    if(serial)
        PRINT_ENTITY("%*s", (int)serial_len, serial);
#undef PRINT_ENTITY

    b3_free(serial, 0);
}

static void notify_all_entities(
    const struct round *restrict round,
    const n3_host *restrict host
) {
    uint8_t buf[N3_SAFE_BUFFER_SIZE];
    int pos = 0;

    b3_for_each_entity(
        round->level.entities,
        notify_entity,
        &(struct notify_entity_data){buf, sizeof(buf), &pos, host}
    );

    if(pos)
        send_notification(buf, (size_t)pos, host);
}

static void process_entities(
    struct round *restrict round,
    const uint8_t *restrict buf,
    size_t size
) {
    int pos = 1; // Skip initial 'e'.

#define SCAN_ENTITIES(...) scan_buffer(buf, &pos, __VA_ARGS__)
    while(buf[pos] == '#') {
        b3_entity_id id = 0;
        int x = 0;
        int y = 0;
        int life = 0;
        size_t serial_len = 0;
        SCAN_ENTITIES(5, "#%X:%X,%X-%X|%zX:", &id, &x, &y, &life, &serial_len);
        const char *serial = (const char *)&buf[pos];
        pos += (int)serial_len;

        if(x < 0 || y < 0 || life < 0 || serial_len > 10000)
            b3_fatal("Received invalid entity data");

        l3_sync_entity(
            id,
            &(b3_pos){x, y},
            life,
            serial,
            serial_len
        );
    }
#undef SCAN_ENTITIES
}

void notify_updates(const struct round *restrict round) {
    // TODO: send out new/modified entities since last run.  Use a flag on each
    // entity to keep track.  Also send out deleted entities (maybe first).
    // Also need a way to set the dirty flag from lua.  When received, call an
    // l3_sync method on lua objects.
}

static void notify_connect(void) {
    uint8_t buf[] = {'c'};
    send_notification(buf, sizeof(buf), NULL);
}

static void process_connect(
    struct round *restrict round,
    const uint8_t *restrict buf,
    size_t size,
    const n3_host *restrict host
) {
    notify_paused_state(round, host);
    notify_map(round, host);
    notify_all_entities(round, host);
}

void process_notifications(struct round *restrict round) {
    uint8_t buf[N3_SAFE_BUFFER_SIZE + 1];
    n3_host host;
    for(
        size_t received;
        (received = receive_notification(
            round,
            buf,
            sizeof(buf) - 1,
            &host
        )) > 0;
    ) {
        buf[received] = 0; // So we can safely scan_buffer() later.

        switch(buf[0]) {
        case 'c': process_connect(round, buf, received, &host); break;
        case 'p': process_paused_state(round, buf, received); break;
        case 'i': process_input(round, buf, received); break;
        case 'm': process_map(round, buf, received); break;
        case 'e': process_entities(round, buf, received); break;
        default: b3_fatal("Received unknown notification");
        }
    }
}

static _Bool filter_connection(
    n3_server *restrict server,
    const n3_host *restrict host,
    void *data
) {
    // const struct round *restrict round = data;

    DEBUG_PRINT("%s connected\n", host_to_string(host));
    return 1;
}

void init_net(void) {
    n3_host host;
    if(args.client || (args.serve && args.hostname))
        n3_init_host(&host, args.hostname, args.port);
    else if(args.serve)
        n3_init_host_any_local(&host, args.port);

    if(args.client) {
        client = n3_new_client(&host);
        DEBUG_PRINT("Connecting to %s\n", host_to_string(&host));
        notify_connect();
    }
    else if(args.serve) {
        server = n3_new_server(&host, filter_connection);
        DEBUG_PRINT("Listening at %s\n", host_to_string(&host));
    }
}

void quit_net(void) {
    n3_free_client(client);
    client = NULL;
    n3_free_server(server);
    server = NULL;
}
