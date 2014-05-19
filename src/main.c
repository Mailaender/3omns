#include "b3/b3.h"
#include "l3/l3.h"
#include "n3/n3.h"

#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>


#define _ // TODO: gettext i18n.

#define RESOURCES "res" // TODO: installed path?

#define FONT_FILENAME RESOURCES "/ttf/Vera.ttf"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define GAME_WIDTH WINDOW_HEIGHT
#define GAME_HEIGHT WINDOW_HEIGHT

#define HEART_SIZE 16

#define DEFAULT_PORT 30325
#define DEFAULT_PORT_STRING B3_STRINGIFY(DEFAULT_PORT)

#define DEBUG_PRINT(...) ((void)(debug && printf(__VA_ARGS__)))

struct debug_stats {
    b3_ticks reset_time;
    int loop_count;
    int update_count;
    int think_count;
    int render_count;
    int skip_count;
    b3_text *text[5];
    b3_rect text_rect[5];
};

// TODO: a Lua debug console directly tied into the Lua environment.


const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static const b3_size window_size = {WINDOW_WIDTH, WINDOW_HEIGHT};
static const b3_size game_size = {GAME_WIDTH, GAME_HEIGHT};
static const b3_rect game_rect = B3_RECT_INIT(0, 0, GAME_WIDTH, GAME_HEIGHT);

static b3_size tile_size = {0, 0};

static _Bool debug = 0;
static _Bool serve = 0;
static _Bool client = 0;
static const char *hostname = NULL;
static uint16_t port = DEFAULT_PORT;

static _Bool paused = 0;

static b3_font *debug_stats_font = NULL;

static b3_text *paused_text = NULL;
static b3_rect paused_text_rect = B3_RECT_INIT(0, 0, 0, 0);

static n3_link *link = NULL;


static error_t parse_uint16(uint16_t *out, const char *string) {
    char *endptr;
    errno = 0;
    long l = strtol(string, &endptr, 10);
    if(errno)
        return errno;
    if(*endptr)
        return EINVAL;
    if(l < 0 || l > UINT16_MAX)
        return ERANGE;
    *out = (uint16_t)l;
    return 0;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch(key) {
    case 'd':
        debug = 1;
        break;
    case 'c':
        client = 1;
        hostname = arg;
        break;
    case 's':
        serve = 1;
        hostname = arg;
        break;
    case 'p':
        {
            error_t e = parse_uint16(&port, arg);
            if(e)
                b3_fatal("Error parsing port '%s': %s", arg, strerror(e));
            break;
        }
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static void parse_args(int argc, char *argv[]) {
    // TODO: I guess argp handles gettext-ing these itself?
    static const char doc[]
        = {"Old-school arcade-style tile-based bomb-dropping deathmatch jam"};
    static struct argp_option options[] = {
        {"debug", 'd', NULL, 0, "Run in debug mode"},
        // TODO: let this be controlled from in-game.
        {NULL, 0, NULL, 0, "Network play options:", 1},
        {"connect", 'c', "server", 0, "Connect to network host", 1},
        {"serve", 's', "from", OPTION_ARG_OPTIONAL,
                "Host network game, listening on the given address "
                "(default: the wildcard address)", 1},
        {"port", 'p', "port", 0, "Port for listening or connecting (default: "
                DEFAULT_PORT_STRING")", 1},
        {0}
    };
    static struct argp argp = {options, parse_opt, NULL, doc};

    error_t e = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if(e)
        b3_fatal("Error parsing arguments: %s", strerror(e));
}

static const char *host_to_string(const n3_host *restrict host) {
    static char string[N3_ADDRESS_SIZE + 10]; // 10 for "UDP |12345".
    char address[N3_ADDRESS_SIZE] = {""};
    n3_get_host_address(host, address, sizeof(address));
    n3_port port = n3_get_host_port(host);
    snprintf(string, sizeof(string), "UDP %s|%"PRIu16, address, port);
    return string;
}

static void init(void) {
    debug_stats_font = b3_load_font(12, FONT_FILENAME, 0);

    b3_font *paused_font = b3_load_font(64, FONT_FILENAME, 0);

    paused_text = b3_new_text(paused_font, "%s", _("P A U S E D"));
    b3_set_text_color(paused_text, 0xbbddcc66);

    b3_size paused_text_size = b3_get_text_size(paused_text);
    paused_text_rect = B3_RECT(
        (window_size.width - paused_text_size.width) / 2,
        (window_size.height - paused_text_size.height) / 2,
        paused_text_size.width,
        paused_text_size.height
    );
    b3_free_font(paused_font);

    n3_host host;
    if(client || (serve && hostname))
        n3_init_host(&host, hostname, port);
    else if(serve)
        n3_init_host_any_local(&host, port);

    if(client) {
        link = n3_new_link(0, &host);
        DEBUG_PRINT("Connecting to %s\n", host_to_string(&host));
    }
    else if(serve) {
        link = n3_new_link(1, &host);
        DEBUG_PRINT("Listening at %s\n", host_to_string(&host));
    }
}

static void quit(void) {
    b3_free_font(debug_stats_font);
    debug_stats_font = NULL;
    b3_free_text(paused_text);
    paused_text = NULL;
    n3_free_link(link);
    link = NULL;
}

static void send_pause_message(_Bool paused) {
    if(!link)
        return;

    n3_message m = {.type = N3_MESSAGE_PAUSE};
    m.pause.paused = paused;
    n3_link_send(link, &m);
}

static void process_message(const n3_message *restrict message) {
    switch(message->type) {
    case N3_MESSAGE_PAUSE:
        paused = message->pause.paused;
        break;
    }
}

static void process_messages(void) {
    if(!link)
        return;

    n3_message message;
    for(n3_message *m; (m = n3_link_receive(link, &message)) != NULL; )
        process_message(m);
}

static _Bool handle_input(b3_input input, _Bool pressed, void *data) {
    l3_level *restrict level = data;

    if(!pressed)
        return 0;

    switch(input) {
    case B3_INPUT_BACK:
        return 1;
    case B3_INPUT_PAUSE:
        paused = !paused;
        send_pause_message(paused);
        return 0;
    default:
        if(!paused)
            l3_input(level, input);
        return 0;
    }
}

static void draw_border(const b3_size *restrict map_size) {
    if(!l3_border_image)
        return;

    b3_rect rect = B3_RECT_INIT(
        game_size.width,
        0,
        tile_size.width,
        tile_size.height
    );
    for(int i = 0; i < map_size->height; i++) {
        b3_draw_image(l3_border_image, &rect);
        rect.y += tile_size.height;
    }
}

static void draw_hearts(l3_level *restrict level) {
    int columns = L3_DUDE_COUNT * 2 - 1;
    int padding = (
        window_size.width
                - game_size.width
                - tile_size.width // For border.
                - columns * HEART_SIZE
    ) / 2;
    b3_rect rect = B3_RECT_INIT(
        game_size.width + tile_size.width + padding,
        0,
        HEART_SIZE,
        HEART_SIZE
    );

    for(int i = 0; i < L3_DUDE_COUNT; i++) {
        if(i > 0)
            rect.x += HEART_SIZE * 2;

        if(!l3_heart_images[i])
            continue;

        b3_entity *dude = b3_get_entity(level->entities, level->dude_ids[i]);
        if(!dude)
            continue;

        rect.y = window_size.height - padding - HEART_SIZE;

        int life = b3_get_entity_life(dude);
        for(int l = 0; l < life; l++) {
            b3_draw_image(l3_heart_images[i], &rect);
            rect.y -= HEART_SIZE;
        }
    }
}

static void draw_debug_stats(struct debug_stats *restrict stats) {
    if(!debug)
        return;

    for(int i = 0; i < 5; i++) {
        if(stats->text[i])
            b3_draw_text(stats->text[i], &stats->text_rect[i]);
    }
}

static void update_debug_stats(
    struct debug_stats *restrict stats,
    b3_ticks elapsed
) {
    stats->reset_time -= elapsed;
    if(stats->reset_time > 0)
        return;

    stats->reset_time = b3_tick_frequency;

    for(int i = 0; i < 5; i++)
        b3_free_text(stats->text[i]);

    stats->text[0]
            = b3_new_text(debug_stats_font, "Loops: %d", stats->loop_count);
    stats->text[1]
            = b3_new_text(debug_stats_font, "Ticks: %d", stats->update_count);
    stats->text[2]
            = b3_new_text(debug_stats_font, "Thinks: %d", stats->think_count);
    stats->text[3]
            = b3_new_text(debug_stats_font, "Draws: %d", stats->render_count);
    stats->text[4]
            = b3_new_text(debug_stats_font, "SKIPS: %d", stats->skip_count);

    for(int i = 0; i < 5; i++)
        b3_set_text_color(stats->text[i], 0xbbffffff);

    int y = 0;
    for(int i = 0; i < 5; i++) {
        b3_size text_size = b3_get_text_size(stats->text[i]);
        stats->text_rect[i] = B3_RECT(
            game_size.width + tile_size.width,
            y,
            text_size.width,
            text_size.height
        );
        y += text_size.height;
    }

    stats->loop_count = 0;
    stats->update_count = 0;
    stats->think_count = 0;
    stats->render_count = 0;
    stats->skip_count = 0;
}

static void loop(l3_level *restrict level) {
    const b3_ticks frame_ticks = b3_secs_to_ticks(0.015625); // 64 FPS.
    const b3_ticks think_ticks = b3_secs_to_ticks(0.0625); // 16 FPS.
    const b3_ticks draw_ticks = b3_secs_to_ticks(0.03125); // 32 FPS.

    struct debug_stats stats = {b3_tick_frequency};

    b3_size map_size = b3_get_map_size(level->map);
    tile_size = b3_get_map_tile_size(&map_size, &game_size);

    // TODO: move these elsewhere.
    l3_agent *agent3 = NULL;
    l3_agent *agent4 = NULL;
    if(!client) {
        agent3 = l3_new_agent(level, 2);
        agent4 = l3_new_agent(level, 3);
    }

    b3_ticks game_ticks = 0;
    b3_ticks ai_ticks = 0;
    b3_ticks ticks = b3_get_tick_count();
    b3_ticks last_ticks;
    b3_ticks next_draw_ticks = 0;
    do {
        last_ticks = ticks;
        ticks = b3_get_tick_count();
        b3_ticks elapsed = ticks - last_ticks;

        if(!paused) {
            int i = 0;
            for(
                game_ticks += elapsed;
                game_ticks >= frame_ticks;
                game_ticks -= frame_ticks, i++
            ) {
                l3_update(level, frame_ticks);

                stats.update_count++;
            }

            if(i > 0)
                stats.skip_count += i - 1;

            if(!client) {
                for(
                    ai_ticks += elapsed;
                    ai_ticks >= think_ticks;
                    ai_ticks -= think_ticks
                ) {
                    l3_think_agent(agent3, think_ticks);
                    l3_think_agent(agent4, think_ticks);

                    stats.think_count++;
                }
            }
        }

        if(ticks >= next_draw_ticks) {
            next_draw_ticks = ticks + draw_ticks;

            b3_begin_scene();

            b3_draw_map(level->map, l3_tile_images, &game_rect);
            draw_border(&map_size);
            b3_draw_entities(level->entities, &game_rect);
            // For now, "sprites" are always on top of the main entities.
            b3_draw_entities(level->sprites, &game_rect);
            draw_hearts(level);

            if(paused)
                b3_draw_text(paused_text, &paused_text_rect);

            stats.render_count++;
            draw_debug_stats(&stats);

            b3_end_scene();
        }

        stats.loop_count++;
        update_debug_stats(&stats, elapsed);

        process_messages();
    } while(!b3_process_events(level));

    l3_free_agent(agent3);
    l3_free_agent(agent4);
}

int main(int argc, char *argv[]) {
    parse_args(argc, argv);

    atexit(b3_quit);
    atexit(l3_quit);
    atexit(quit);

    b3_init("3omns", &window_size, handle_input);
    l3_init(RESOURCES, debug);
    init();

    l3_level level = l3_generate();

    loop(&level);

    l3_free_level(&level);
    return 0;
}
