#include "3omns.h"
#include "b3/b3.h"
#include "l3/l3.h"
#include "n3/n3.h"

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>


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
    size_t size
) {
    size_t received = 0;
    if(args.client) {
        received = n3_client_receive(client, buf, size);
        if(received)
            debug_network_print(buf, received, "Received: ");
    }
    else if(args.serve) {
        n3_host host;
        received = n3_server_receive(server, buf, size, &host, round);
        if(received) {
            debug_network_print(
                buf,
                received,
                "Received from %s: ",
                host_to_string(&host)
            );
        }
    }
    return received;
}

// TODO: this won't be necessary once the n3 protocol is finished.
static void notify_connect(void) {
    uint8_t buf[] = {'c'};
    send_notification(buf, sizeof(buf), NULL);
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
    if(size >= 2)
        round->paused = (buf[1] == '0' ? 0 : 1);
}

void notify_paused_changed(const struct round *restrict round) {
    notify_paused_state(round, NULL);
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
                b3_fatal("Too much data for map");

            b3_set_map_tile(round->level.map, &map_pos, run_tile);
            if(++(map_pos.x) >= width) {
                map_pos.x = 0;
                map_pos.y++;
            }
        }
    } while(buf[pos++] == ';');
#undef SCAN_MAP

    l3_set_sync_level(&round->level);

    round->initialized = 1;
}

void process_notifications(struct round *restrict round) {
    uint8_t buf[N3_SAFE_BUFFER_SIZE + 1];
    for(
        size_t received;
        (received = receive_notification(round, buf, sizeof(buf) - 1)) > 0;
    ) {
        buf[received] = 0; // So we can safely scan_buffer() later.

        switch(buf[0]) {
        case 'c': /* "Connect"; do nothing. */ break;
        case 'p': process_paused_state(round, buf, received); break;
        case 'm': process_map(round, buf, received); break;
        }
    }
}

static _Bool filter_connection(
    n3_server *restrict server,
    const n3_host *restrict host,
    void *data
) {
    const struct round *restrict round = data;

    DEBUG_PRINT("%s connected\n", host_to_string(host));

    notify_paused_state(round, host);
    notify_map(round, host);

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
