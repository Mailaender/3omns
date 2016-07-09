/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <https://chazomaticus.github.io/3omns/>
    Copyright 2014-2016 Charles Lindsay <chaz@chazomatic.us>

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

#include <stddef.h>
#include <stdlib.h>


#define _ // TODO: gettext i18n.

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define GAME_WIDTH WINDOW_HEIGHT
#define GAME_HEIGHT WINDOW_HEIGHT

#define HEART_SIZE 16


const b3_size window_size = {WINDOW_WIDTH, WINDOW_HEIGHT};
const b3_size game_size = {GAME_WIDTH, GAME_HEIGHT};
const b3_rect game_rect = B3_RECT_INIT(0, 0, GAME_WIDTH, GAME_HEIGHT);
const b3_size heart_size = {HEART_SIZE, HEART_SIZE};

struct args args = ARGS_INIT_DEFAULT;

static b3_font *debug_stats_font = NULL;

static b3_text *paused_text = NULL;
static b3_rect paused_text_rect = B3_RECT_INIT(0, 0, 0, 0);


static char *resource(const char *restrict filename) {
    return b3_copy_format("%s%s", args.resources, filename);
}

static void init_res(void) {
    char *font_filename = resource("/ttf/Vera.ttf");

    debug_stats_font = b3_load_font(12, font_filename, 0);
    b3_font *paused_font = b3_load_font(64, font_filename, 0);

    b3_free(font_filename, 0);

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

static void pause(struct round *restrict round, _Bool p) {
    if(round->paused != p) {
        round->paused = p;
        notify_paused_changed(round);
    }
}

static _Bool handle_input(b3_input input, _Bool pressed, void *data) {
    struct round *restrict round = data;

    if(!pressed)
        return 0;

    switch(input) {
    case B3_INPUT_BACK:
        return 1;
    case B3_INPUT_PAUSE:
        pause(round, !round->paused);
        return 0;
    case B3_INPUT_DEBUG:
        if(args.debug) {
            pause(round, 1);
            l3_enter_debugger();
        }
        return 0;
    default:
        notify_input(round, input);
        // TODO: it would be nice to be able to keep the client-side code
        // running as normal, in anticipation of server updates.  That might
        // require a bit more sophisticated syncing.
        if(!round->paused && !args.client)
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
                - columns * heart_size.width
    ) / 2;
    b3_rect rect = B3_RECT_INIT(
        game_size.width + round->tile_size.width + padding,
        0,
        heart_size.width,
        heart_size.height
    );

    for(int i = 0; i < L3_DUDE_COUNT; i++) {
        if(i > 0)
            rect.x += heart_size.width * 2;

        if(!l3_heart_images[i])
            continue;

        b3_entity *dude = b3_get_entity(
            round->level.entities,
            round->level.dude_ids[i]
        );
        if(!dude)
            continue;

        rect.y = window_size.height - padding - heart_size.height;

        int life = b3_get_entity_life(dude);
        for(int l = 0; l < life; l++) {
            b3_draw_image(l3_heart_images[i], &rect);
            rect.y -= heart_size.height;
        }
    }
}

static void free_debug_stats(struct debug_stats *restrict stats) {
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(stats->text); i++)
        b3_free_text(stats->text[i]);
}

static void draw_debug_stats(struct debug_stats *restrict stats) {
    if(!args.debug)
        return;

    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(stats->text); i++) {
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

    get_net_debug_stats(stats);

    free_debug_stats(stats);

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
    stats->text[5]
            = b3_new_text(debug_stats_font, "Sent: %d", stats->sent_packets);
    stats->text[6] = b3_new_text(
        debug_stats_font,
        "Rec'd: %d",
        stats->received_packets
    );

    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(stats->text); i++)
        b3_set_text_color(stats->text[i], 0xbbffffff);

    int y = 0;
    for(int i = 0; i < B3_STATIC_ARRAY_COUNT(stats->text); i++) {
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

            // TODO: theoretically a client can run an agent.  Agents would
            // need to be modified to plug into l3_input (and sync'd to the
            // server) instead of just manipulating the entities inside Lua.
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

        if(round->initialized && (
            b3_get_entity_pool_dirty(round->level.sprites)
                    || b3_get_entity_pool_dirty(round->level.entities)
        )) {
            l3_cull(&round->level);

            notify_updates(round);

            b3_set_entity_pool_dirty(round->level.sprites, 0);
            b3_set_entity_pool_dirty(round->level.entities, 0);
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

        update_net(round);
        // TODO: poll() with an intelligent timeout here.
        process_notifications(round);
    } while(!b3_process_events(round));

    free_debug_stats(&stats);
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
    parse_args(&args, argc, argv);

    DEBUG_PRINT("%s\n", PACKAGE_STRING);
    DEBUG_PRINT("Resources: %s\n", args.resources);
    DEBUG_PRINT("Game: %s\n", args.game);

    atexit(b3_quit);
    atexit(quit_net);
    atexit(b3_exit_window);
    atexit(l3_quit);
    atexit(quit_res);

    b3_init();
    init_net();
    b3_enter_window(PACKAGE_NAME, &window_size, handle_input);
    l3_init(args.resources, args.game, args.client, args.debug);
    init_res();

    struct round round = ROUND_INIT;
    new_round(&round);

    loop(&round);

    free_round(&round);
    return 0;
}
