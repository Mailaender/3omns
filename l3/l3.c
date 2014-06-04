/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
    l3 - Lua interface library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
*/

#include "l3.h"
#include "internal.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdio.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <lua5.2/lualib.h>


struct update_entity_data {
    lua_Number elapsed;
};

enum action {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    FIRE,
};


b3_image *l3_tile_images[B3_TILE_COUNT] = {NULL};
b3_image *l3_border_image = NULL;
b3_image *l3_heart_images[L3_DUDE_COUNT] = {NULL};

char *resource_path = NULL;
char *game = NULL;
_Bool client = 0;
_Bool debug = 0;
lua_State *lua = NULL;


static int open_all(lua_State *restrict l) {
    const luaL_Reg submodules[] = {
        {IMAGE_NAME, open_image},
        {LEVEL_NAME, open_level},
        {NULL, NULL}
    };

    luaL_newlibtable(l, submodules);
    for(const luaL_Reg *submodule = submodules; submodule->func; submodule++) {
        submodule->func(l);
        lua_setfield(l, -2, submodule->name);
    }

    lua_pushstring(l, resource_path);
    lua_setfield(l, -2, "RESOURCE_PATH");
    lua_pushstring(l, game);
    lua_setfield(l, -2, "GAME");
    lua_pushboolean(l, client);
    lua_setfield(l, -2, "CLIENT");
    lua_pushboolean(l, debug);
    lua_setfield(l, -2, "DEBUG");

    return 1;
}

static void *lalloc(
    void *restrict ud,
    void *restrict ptr,
    size_t osize,
    size_t nsize
) {
    if(nsize)
        return b3_realloc(ptr, nsize);

    b3_free(ptr, 0);
    return NULL;
}

_Noreturn static int panic(lua_State *restrict l) {
    if(debug) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(l, -1));
        l3_enter_debugger();
        b3_fatal("Terminating");
    }
    else
        b3_fatal("Lua error: %s", lua_tostring(l, -1));
}

static lua_State *new_lua(void) {
    lua_State *l = lua_newstate(lalloc, NULL);
    if(!l)
        b3_fatal("Error creating Lua context");

    lua_atpanic(l, panic);
    luaL_openlibs(l);

    luaL_requiref(l, L3_NAME, open_all, 1);
    lua_pop(l, 1);

    return l;
}

static void run_game_file(lua_State *restrict l, const char *restrict base) {
    char *filename = b3_copy_format("%s/%s/%s.lua", resource_path, game, base);
    if(luaL_dofile(l, filename))
        b3_fatal("Error running game file %s: %s", base, lua_tostring(l, -1));
    b3_free(filename, 0);
}

static void set_tile_images(lua_State *restrict l) {
    lua_getglobal(l, L3_TILE_IMAGES_NAME);
    if(!lua_istable(l, -1))
        b3_fatal("Missing global table %s", L3_TILE_IMAGES_NAME);

    for(int i = 0; i < B3_TILE_COUNT; i++) {
        lua_pushunsigned(l, (lua_Unsigned)i);
        lua_gettable(l, -2);

        b3_image **p_image = luaL_testudata(l, -1, IMAGE_METATABLE);
        if(p_image)
            l3_tile_images[i] = b3_ref_image(*p_image);

        lua_pop(l, 1);
    }
    lua_pop(l, 1);
}

static void set_border_image(lua_State *restrict l) {
    lua_getglobal(l, L3_BORDER_IMAGE_NAME);
    b3_image **p_image = luaL_testudata(l, -1, IMAGE_METATABLE);
    if(p_image)
        l3_border_image = b3_ref_image(*p_image);

    lua_pop(l, 1);
}

static void set_heart_images(lua_State *restrict l) {
    lua_getglobal(l, L3_HEART_IMAGES_NAME);
    if(!lua_istable(l, -1))
        b3_fatal("Missing global table %s", L3_HEART_IMAGES_NAME);

    for(int i = 0; i < L3_DUDE_COUNT; i++) {
        lua_pushinteger(l, (lua_Integer)(i + 1));
        lua_gettable(l, -2);

        b3_image **p_image = luaL_testudata(l, -1, IMAGE_METATABLE);
        if(p_image)
            l3_heart_images[i] = b3_ref_image(*p_image);

        lua_pop(l, 1);
    }
    lua_pop(l, 1);
}

void l3_init(
    const char *restrict resource_path_,
    const char *restrict game_,
    _Bool client_,
    _Bool debug_
) {
    resource_path = b3_copy_string(resource_path_);
    game = b3_copy_string(game_);
    client = client_;
    debug = debug_;

    lua = new_lua();
    run_game_file(lua, "init");

    set_tile_images(lua);
    set_border_image(lua);
    set_heart_images(lua);
}

void l3_quit(void) {
    for(int i = 0; i < B3_TILE_COUNT; i++) {
        b3_free_image(l3_tile_images[i]);
        l3_tile_images[i] = NULL;
    }
    b3_free_image(l3_border_image);
    for(int i = 0; i < L3_DUDE_COUNT; i++) {
        b3_free_image(l3_heart_images[i]);
        l3_heart_images[i] = NULL;
    }
    l3_border_image = NULL;
    luaL_unref(lua, LUA_REGISTRYINDEX, sync_level_ref);
    sync_level_ref = LUA_NOREF;
    if(lua) {
        lua_close(lua);
        lua = NULL;
    }
    b3_free(resource_path, 0);
    resource_path = NULL;
    b3_free(game, 0);
    game = NULL;
}

void l3_enter_debugger(void) {
    puts("Entering Lua debugging session; enter cont to resume");
    lua_getglobal(lua, "debug");
    lua_getfield(lua, -1, "debug");
    lua_call(lua, 0, 0);
    lua_pop(lua, 1);
}

static void clean_entity(b3_entity *restrict entity, void *callback_data) {
    b3_set_entity_dirty(entity, 0);
}

l3_level l3_generate(void) {
    lua_getglobal(lua, L3_GENERATE_NAME);
    if(!lua_isfunction(lua, -1))
        b3_fatal("Missing global function %s", L3_GENERATE_NAME);

    lua_call(lua, 0, 1); // Rely on panic to handle errors.

    l3_level *level = luaL_testudata(lua, -1, LEVEL_METATABLE);
    if(!level)
        b3_fatal("%s didn't return a level", L3_GENERATE_NAME);
    if(!level->dude_ids[0])
        b3_fatal("%s didn't fill in at least one dude id", L3_GENERATE_NAME);

    l3_cull(level);
    b3_for_each_entity(level->entities, clean_entity, NULL);
    b3_set_entity_pool_dirty(level->entities, 0);

    l3_level copy;
    l3_copy_level(&copy, level);

    lua_pop(lua, 1);

    return copy;
}

static void update_entity(b3_entity *restrict entity, void *callback_data) {
    const struct update_entity_data *restrict update_data = callback_data;

    const struct entity_data *entity_data = b3_get_entity_data(entity);
    lua_State *l = entity_data->l;

    if(entity_data->context_ref < 0 || !b3_get_entity_life(entity))
        return;

    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    lua_getfield(l, -1, L3_ENTITY_UPDATE_NAME);
    if(!lua_isfunction(l, -1)) {
        lua_pop(l, 2);
        return;
    }

    lua_insert(l, -2);
    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->entity_ref);
    lua_pushnumber(l, update_data->elapsed);

    lua_call(l, 3, 0);
}

void l3_update(l3_level *restrict level, b3_ticks elapsed) {
    struct update_entity_data update_data = {
        (lua_Number)b3_ticks_to_secs(elapsed),
    };

    b3_for_each_entity(level->sprites, update_entity, &update_data);
    b3_for_each_entity(level->entities, update_entity, &update_data);
}

static void cull_entity(b3_entity *restrict entity, void *callback_data) {
    if(!b3_get_entity_life(entity))
        b3_release_entity(entity);
}

void l3_cull(l3_level *restrict level) {
    b3_for_each_entity(level->sprites, cull_entity, NULL);
    b3_for_each_entity(level->entities, cull_entity, NULL);
}

static enum action input_to_action(b3_input input, int player) {
    if(input == B3_INPUT_UP(player)) return UP;
    if(input == B3_INPUT_DOWN(player)) return DOWN;
    if(input == B3_INPUT_LEFT(player)) return LEFT;
    if(input == B3_INPUT_RIGHT(player)) return RIGHT;
    return FIRE;
}

static const char *action_to_string(enum action action) {
    switch(action) {
    case UP: return "u";
    case DOWN: return "d";
    case LEFT: return "l";
    case RIGHT: return "r";
    default: return "f";
    }
}

static void entity_action(
    lua_State *restrict l,
    b3_entity *restrict entity,
    enum action action
) {
    const struct entity_data *entity_data = b3_get_entity_data(entity);

    if(entity_data->context_ref < 0 || !b3_get_entity_life(entity))
        return;

    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    lua_getfield(l, -1, L3_ENTITY_ACTION_NAME);
    if(!lua_isfunction(l, -1)) {
        lua_pop(l, 2);
        return;
    }

    lua_insert(l, -2);
    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->entity_ref);
    lua_pushlstring(l, action_to_string(action), 1);

    lua_call(l, 3, 0);
}

void l3_input(l3_level *restrict level, b3_input input) {
    if(!B3_INPUT_IS_PLAYER(input))
        return;

    int i = B3_INPUT_PLAYER(input);
    b3_entity *entity = b3_get_entity(level->entities, level->dude_ids[i]);
    if(entity)
        entity_action(lua, entity, input_to_action(input, i));
}
