#include "3omns.h"
#include "b3/b3.h"
#include "l3/l3.h"
#include "n3/n3.h"

#include <errno.h>
#include <string.h>
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

struct debug_stats {
    b3_ticks reset_time;
    int loop_count;
    int update_count;
    int think_count;
    int render_count;
    int skip_count;
    // TODO: net statistics.
    b3_text *text[5];
    b3_rect text_rect[5];
};

// TODO: a Lua debug console directly tied into the Lua environment.


const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

const b3_size window_size = {WINDOW_WIDTH, WINDOW_HEIGHT};
const b3_size game_size = {GAME_WIDTH, GAME_HEIGHT};
const b3_rect game_rect = B3_RECT_INIT(0, 0, GAME_WIDTH, GAME_HEIGHT);

struct args args = ARGS_INIT_DEFAULT;

static b3_font *debug_stats_font = NULL;

static b3_text *paused_text = NULL;
static b3_rect paused_text_rect = B3_RECT_INIT(0, 0, 0, 0);


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

static void parse_args(int argc, char *argv[]) {
    // TODO: I guess argp handles gettext-ing these itself?
    static const char doc[]
        = {"Old-school arcade-style tile-based bomb-dropping deathmatch jam"};
    static struct argp_option options[] = {
        {"debug", 'd', NULL, 0, "Run in debug mode"},
        {"debug-network", 'n', NULL, 0, "Print network communication"},
        // TODO: let this be controlled from in-game.
        {NULL, 0, NULL, 0, "Network play options:", 1},
        {"connect", 'c', "server", 0, "Connect to network host", 1},
        {"serve", 's', "from", OPTION_ARG_OPTIONAL,
                "Host network game, listening on the given address "
                "(default: the wildcard address)", 1},
        {"port", 'p', "port", 0, "Port for listening or connecting (default: "
                B3_STRINGIFY(DEFAULT_PORT)")", 1},
        {0}
    };
    static struct argp argp = {options, parse_opt, NULL, doc};

    error_t e = argp_parse(&argp, argc, argv, 0, NULL, &args);
    if(e)
        b3_fatal("Error parsing arguments: %s", strerror(e));
}

static void init_res(void) {
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
}

static void quit_res(void) {
    b3_free_font(debug_stats_font);
    debug_stats_font = NULL;
    b3_free_text(paused_text);
    paused_text = NULL;
}

static _Bool handle_input(b3_input input, _Bool pressed, void *data) {
    struct round *restrict round = data;

    if(!pressed)
        return 0;

    switch(input) {
    case B3_INPUT_BACK:
        return 1;
    case B3_INPUT_PAUSE:
        round->paused = !round->paused;
        notify_paused(round, NULL);
        return 0;
    default:
        if(!round->paused)
            l3_input(&round->level, input);
        return 0;
    }
}

static void draw_border(const struct round *restrict round) {
    if(!l3_border_image)
        return;

    b3_rect rect = B3_RECT_INIT(
        game_size.width,
        0,
        round->tile_size.width,
        round->tile_size.height
    );
    for(int i = 0; i < round->map_size.height; i++) {
        b3_draw_image(l3_border_image, &rect);
        rect.y += round->tile_size.height;
    }
}

static void draw_hearts(const struct round *restrict round) {
    int columns = L3_DUDE_COUNT * 2 - 1;
    int padding = (
        window_size.width
                - game_size.width
                - round->tile_size.width // For border.
                - columns * HEART_SIZE
    ) / 2;
    b3_rect rect = B3_RECT_INIT(
        game_size.width + round->tile_size.width + padding,
        0,
        HEART_SIZE,
        HEART_SIZE
    );

    for(int i = 0; i < L3_DUDE_COUNT; i++) {
        if(i > 0)
            rect.x += HEART_SIZE * 2;

        if(!l3_heart_images[i])
            continue;

        b3_entity *dude = b3_get_entity(
            round->level.entities,
            round->level.dude_ids[i]
        );
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
    if(!args.debug)
        return;

    for(int i = 0; i < 5; i++) {
        if(stats->text[i])
            b3_draw_text(stats->text[i], &stats->text_rect[i]);
    }
}

static void update_debug_stats(
    const struct round *restrict round,
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
            game_size.width + round->tile_size.width,
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
    // Don't reset skip_count.
}

static void loop(struct round *restrict round) {
    const b3_ticks frame_ticks = b3_secs_to_ticks(0.015625); // 64 FPS.
    const b3_ticks think_ticks = b3_secs_to_ticks(0.0625); // 16 FPS.
    const b3_ticks draw_ticks = b3_secs_to_ticks(0.03125); // 32 FPS.

    struct debug_stats stats = {b3_tick_frequency};

    b3_ticks game_ticks = 0;
    b3_ticks ai_ticks = 0;
    b3_ticks ticks = b3_get_tick_count();
    b3_ticks last_ticks;
    b3_ticks next_draw_ticks = 0;
    do {
        last_ticks = ticks;
        ticks = b3_get_tick_count();
        b3_ticks elapsed = ticks - last_ticks;

        if(round->initialized && !round->paused) {
            int i = 0;
            for(
                game_ticks += elapsed;
                game_ticks >= frame_ticks;
                game_ticks -= frame_ticks, i++
            ) {
                l3_update(&round->level, frame_ticks);

                stats.update_count++;
            }

            if(i > 0)
                stats.skip_count += i - 1;

            if(!args.client) {
                for(
                    ai_ticks += elapsed;
                    ai_ticks >= think_ticks;
                    ai_ticks -= think_ticks
                ) {
                    for(int i = 0; i < L3_DUDE_COUNT && round->agents[i]; i++)
                        l3_think_agent(round->agents[i], think_ticks);

                    stats.think_count++;
                }
            }
        }

        if(ticks >= next_draw_ticks) {
            next_draw_ticks = ticks + draw_ticks;

            b3_begin_scene();

            if(round->initialized) {
                b3_draw_map(round->level.map, l3_tile_images, &game_rect);
                b3_draw_entities(round->level.entities, &game_rect);
                // For now, "sprites" are always on top of the main entities.
                b3_draw_entities(round->level.sprites, &game_rect);
                draw_border(round);
                draw_hearts(round);
            }

            if(round->paused)
                b3_draw_text(paused_text, &paused_text_rect);

            stats.render_count++;
            draw_debug_stats(&stats);

            b3_end_scene();
        }

        stats.loop_count++;
        update_debug_stats(round, &stats, elapsed);

        process_notifications(round);
    } while(!b3_process_events(round));
}

static void free_round(struct round *restrict round) {
    for(int i = 0; i < L3_DUDE_COUNT; i++)
        l3_free_agent(round->agents[i]);
    l3_free_level(&round->level);
    *round = (struct round)ROUND_INIT;
}

static void new_round(struct round *restrict round) {
    free_round(round);

    if(!args.client) {
        round->level = l3_generate();
        round->map_size = b3_get_map_size(round->level.map);
        round->tile_size = b3_get_map_tile_size(&round->map_size, &game_size);

        round->agents[0] = l3_new_agent(&round->level, 2);
        round->agents[1] = l3_new_agent(&round->level, 3);

        round->initialized = 1;
    }
}

int main(int argc, char *argv[]) {
    parse_args(argc, argv);

    atexit(b3_quit);
    atexit(quit_net);
    atexit(l3_quit);
    atexit(quit_res);

    b3_init("3omns", &window_size, handle_input);
    init_net();
    l3_init(RESOURCES, args.debug);
    init_res();

    struct round round = ROUND_INIT;
    new_round(&round);

    loop(&round);

    free_round(&round);
    return 0;
}
