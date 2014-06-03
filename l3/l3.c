#include "l3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdio.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <lua5.2/lualib.h>


// TODO: split this file up.  It's getting a bit unwieldy.

#define L3_NAME "l3"
#define IMAGE_NAME "image"
#define LEVEL_NAME "level"

#define IMAGE_METATABLE L3_NAME "." IMAGE_NAME
#define LEVEL_METATABLE L3_NAME "." LEVEL_NAME
#define SPRITE_METATABLE L3_NAME ".sprite"
#define ENTITY_METATABLE L3_NAME ".entity"

#define L3_GENERATE_NAME "l3_generate"
#define L3_SYNC_NAME "l3_sync"
#define L3_SYNC_DELETED_NAME "l3_sync_deleted"

#define L3_TILE_IMAGES_NAME "L3_TILE_IMAGES"
#define L3_BORDER_IMAGE_NAME "L3_BORDER_IMAGE"
#define L3_HEART_IMAGES_NAME "L3_HEART_IMAGES"

#define L3_ENTITY_UPDATE_NAME "l3_update"
#define L3_ENTITY_ACTION_NAME "l3_action"
#define L3_ENTITY_SERIALIZE_NAME "l3_serialize"
#define L3_ENTITY_THINK_AGENT_NAME "l3_think"


struct l3_agent {
    int ref_count;
    b3_entity_pool *entities;
    b3_entity_id dude_id;
    _Bool first;
    _Bool done;
    lua_State *thread;
    int thread_ref;
};

struct entity_data {
    lua_State *l;
    int entity_ref; // Lua reference to the entity userdata.
    int context_ref; // Lua reference to the context object.
    b3_map *map;
};

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

static char *resource_path = NULL;
static _Bool client = 0;
static _Bool debug = 0;
static lua_State *lua = NULL;
static int sync_level_ref = LUA_NOREF;


static void push_pos(lua_State *restrict l, const b3_pos *restrict pos) {
    lua_createtable(l, 0, 2);
    lua_pushinteger(l, (lua_Integer)(pos->x + 1));
    lua_setfield(l, -2, "x");
    lua_pushinteger(l, (lua_Integer)(pos->y + 1));
    lua_setfield(l, -2, "y");
}

static b3_pos check_pos(lua_State *restrict l, int arg_index) {
    luaL_checktype(l, arg_index, LUA_TTABLE);
    lua_getfield(l, arg_index, "x");
    lua_getfield(l, arg_index, "y");

    int x_is_num, y_is_num;
    int x = (int)lua_tointegerx(l, -2, &x_is_num) - 1;
    int y = (int)lua_tointegerx(l, -1, &y_is_num) - 1;
    luaL_argcheck(
        l,
        x_is_num && y_is_num,
        arg_index,
        "pos requires integer x and y fields"
    );

    lua_pop(l, 2);
    return (b3_pos){x, y};
}

static void push_size(lua_State *restrict l, const b3_size *restrict size) {
    lua_createtable(l, 0, 2);
    lua_pushinteger(l, (lua_Integer)size->width);
    lua_setfield(l, -2, "width");
    lua_pushinteger(l, (lua_Integer)size->height);
    lua_setfield(l, -2, "height");
}

static b3_size check_size(lua_State *restrict l, int arg_index) {
    luaL_checktype(l, arg_index, LUA_TTABLE);
    lua_getfield(l, arg_index, "width");
    lua_getfield(l, arg_index, "height");

    int width_is_num, height_is_num;
    int width = (int)lua_tointegerx(l, -2, &width_is_num);
    int height = (int)lua_tointegerx(l, -1, &height_is_num);
    luaL_argcheck(
        l,
        width_is_num && height_is_num,
        arg_index,
        "size requires integer width and height fields"
    );

    lua_pop(l, 2);
    return (b3_size){width, height};
}

static b3_rect check_rect(lua_State *restrict l, int arg_index) {
    luaL_checktype(l, arg_index, LUA_TTABLE);
    lua_getfield(l, arg_index, "x");
    lua_getfield(l, arg_index, "y");
    lua_getfield(l, arg_index, "width");
    lua_getfield(l, arg_index, "height");

    int x_is_num, y_is_num, width_is_num, height_is_num;
    int x = (int)lua_tointegerx(l, -4, &x_is_num);
    int y = (int)lua_tointegerx(l, -3, &y_is_num);
    int width = (int)lua_tointegerx(l, -2, &width_is_num);
    int height = (int)lua_tointegerx(l, -1, &height_is_num);
    luaL_argcheck(
        l,
        x_is_num && y_is_num && width_is_num && height_is_num,
        arg_index,
        "rect requires integer x, y, width, and height fields"
    );

    lua_pop(l, 4);
    return B3_RECT(x, y, width, height);
}

static b3_image **push_new_image(lua_State *restrict l) {
    b3_image **p_image = lua_newuserdata(l, sizeof(*p_image));
    luaL_setmetatable(l, IMAGE_METATABLE);

    return p_image;
}

static b3_image *check_image(lua_State *restrict l, int index) {
    return *(b3_image **)luaL_checkudata(l, index, IMAGE_METATABLE);
}

static int image_load(lua_State *restrict l) {
    const char *filename = luaL_checkstring(l, 1);
    luaL_argcheck(l, filename && *filename, 1, "filename required");

    b3_image **p_image = push_new_image(l);
    *p_image = b3_load_image(filename);
    return 1;
}

static int image_gc(lua_State *restrict l) {
    b3_image *image = check_image(l, 1);
    b3_free_image(image);
    return 0;
}

static int image_sub(lua_State *restrict l) {
    b3_image *image = check_image(l, 1);
    b3_rect rect = check_rect(l, 2);

    b3_image **p_image = push_new_image(l);
    *p_image = b3_new_sub_image(image, &rect);
    return 1;
}

static int open_image(lua_State *restrict l) {
    static const luaL_Reg functions[] = {
        {"load", image_load},
        {NULL, NULL}
    };
    static const luaL_Reg methods[] = {
        {"sub", image_sub},
        {NULL, NULL}
    };

    luaL_newmetatable(l, IMAGE_METATABLE);

    lua_pushcfunction(l, image_gc);
    lua_setfield(l, -2, "__gc");
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, methods, 0);
    lua_pop(l, 1);

    luaL_newlib(l, functions);
    return 1;
}

static l3_level *check_level(lua_State *restrict l, int index) {
    return luaL_checkudata(l, index, LEVEL_METATABLE);
}

static l3_level *push_new_level(lua_State *restrict l) {
    l3_level *level = lua_newuserdata(l, sizeof(*level));
    luaL_setmetatable(l, LEVEL_METATABLE);

    return level;
}

static int level_new(lua_State *restrict l) {
    b3_size size = check_size(l, 1);
    int max_entities = luaL_checkint(l, 2);

    l3_level *level = push_new_level(l);

    l3_init_level(level, client, &size, max_entities);
    return 1;
}

static int level_gc(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    l3_free_level(level);
    return 0;
}

static int level_get_size(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);

    b3_size size = b3_get_map_size(level->map);
    push_size(l, &size);
    return 1;
}

static b3_pos check_map_pos(
    lua_State *restrict l,
    int arg_index,
    b3_map *restrict map
) {
    b3_pos pos = check_pos(l, arg_index);

    b3_size size = b3_get_map_size(map);
    luaL_argcheck(
        l,
        pos.x >= 0 && pos.x < size.width && pos.y >= 0 && pos.y < size.height,
        arg_index,
        "pos fields must satisfy 1 <= pos <= level:get_size()"
    );

    return pos;
}

static int level_get_tile(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    b3_pos pos = check_map_pos(l, 2, level->map);

    lua_pushunsigned(l, (lua_Unsigned)b3_get_map_tile(level->map, &pos));
    return 1;
}

static int level_set_tile(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    b3_pos pos = check_map_pos(l, 2, level->map);
    b3_tile tile = (b3_tile)luaL_checkunsigned(l, 3);

    b3_set_map_tile(level->map, &pos, tile);

    lua_pushvalue(l, 1);
    return 1;
}

static struct entity_data *new_entity_data(
    lua_State *restrict l,
    int entity_index,
    b3_map *restrict map
) {
    struct entity_data *entity_data = b3_malloc(sizeof(*entity_data), 1);
    entity_data->l = l;

    // We're using the Lua state-global registry to keep references to the
    // entity userdata and its context (the Lua object passed to
    // l3.level.new_sprite/_entity).  There may be a more elegant way to go
    // about this, but this seemed pretty simple and I couldn't see any major
    // problems, so I went with it.
    lua_pushvalue(l, entity_index);
    entity_data->entity_ref = luaL_ref(l, LUA_REGISTRYINDEX);
    entity_data->context_ref = LUA_NOREF;

    entity_data->map = b3_ref_map(map);
    return entity_data;
}

static struct entity_data *ref_entity_context(
    lua_State *restrict l,
    struct entity_data *entity_data,
    int context_index
) {
    luaL_unref(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    entity_data->context_ref = LUA_NOREF;

    lua_pushvalue(l, context_index);
    if(lua_istable(l, -1))
        entity_data->context_ref = luaL_ref(l, LUA_REGISTRYINDEX);
    else
        lua_pop(l, 1);

    return entity_data;
}

static void free_entity_data(b3_entity *restrict entity, void *entity_data) {
    struct entity_data *restrict d = entity_data;
    if(d) {
        luaL_unref(d->l, LUA_REGISTRYINDEX, d->entity_ref);
        luaL_unref(d->l, LUA_REGISTRYINDEX, d->context_ref);
        b3_free_map(d->map);
        b3_free(d, sizeof(*d));
    }
}

static b3_entity *check_sprite(lua_State *restrict l, int index) {
    return *(b3_entity **)luaL_checkudata(l, index, SPRITE_METATABLE);
}

static int level_new_sprite(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    // Index 2 is the Lua sprite object, passed to ref_entity_context() below.

    // "Sprites" are actually entities, but that don't have life in Lua, nor
    // are they sync'd to the server.
    b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
    luaL_setmetatable(l, SPRITE_METATABLE);

    *p_entity = b3_claim_entity(level->sprites, 0, free_entity_data);
    b3_set_entity_life(*p_entity, 1);
    struct entity_data *entity_data = new_entity_data(l, -1, level->map);
    b3_set_entity_data(*p_entity, ref_entity_context(l, entity_data, 2));
    return 1;
}

static int sprite_destroy(lua_State *restrict l) {
    b3_entity *entity = check_sprite(l, 1);

    b3_set_entity_life(entity, 0);

    return 0;
}

// Works on either "sprites" or entities.
static b3_entity *check_sprentity(lua_State *restrict l, int index) {
    b3_entity **p_entity = luaL_testudata(l, index, SPRITE_METATABLE);
    if(p_entity)
        return *p_entity;
    p_entity = luaL_testudata(l, index, ENTITY_METATABLE);
    if(p_entity)
        return *p_entity;

    const char *message = lua_pushfstring(
        l,
        "sprite or entity expected, got %s",
        luaL_typename(l, index)
    );
    luaL_argerror(l, index, message);
    return NULL;
}

// Works on either "sprites" or entities.
static int sprentity_get_pos(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);

    b3_pos pos = b3_get_entity_pos(entity);
    push_pos(l, &pos);
    return 1;
}

// Works on either "sprites" or entities.
static int sprentity_set_pos(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);
    struct entity_data *entity_data = b3_get_entity_data(entity);
    b3_pos pos = check_map_pos(l, 2, entity_data->map);

    b3_set_entity_pos(entity, &pos);

    lua_pushvalue(l, 1);
    return 1;
}

// Works on either "sprites" or entities.
static int sprentity_set_image(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);
    luaL_checkany(l, 2);
    b3_image *image = NULL;
    if(!lua_isnil(l, 2))
        image = check_image(l, 2);

    b3_set_entity_image(entity, image);

    lua_pushvalue(l, 1);
    return 1;
}

// Works on either "sprites" or entities.
static int sprentity_set_z_order(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);
    int z_order = luaL_checkint(l, 2);

    b3_set_entity_z_order(entity, z_order);

    lua_pushvalue(l, 1);
    return 1;
}

static b3_entity *check_entity(lua_State *restrict l, int index) {
    return *(b3_entity **)luaL_checkudata(l, index, ENTITY_METATABLE);
}

static b3_entity **push_new_entity(lua_State *restrict l) {
    b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
    luaL_setmetatable(l, ENTITY_METATABLE);

    return p_entity;
}

static int level_new_entity(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    // Index 2 is the Lua entity object, passed to ref_entity_context() below.

    b3_entity **p_entity = push_new_entity(l);
    *p_entity = b3_claim_entity(level->entities, 0, free_entity_data);
    struct entity_data *entity_data = new_entity_data(l, -1, level->map);
    b3_set_entity_data(*p_entity, ref_entity_context(l, entity_data, 2));
    return 1;
}

static int level_get_entity(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    b3_entity_id id = (b3_entity_id)luaL_checkunsigned(l, 2);

    b3_entity *entity = b3_get_entity(level->entities, id);
    if(entity) {
        b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
        luaL_setmetatable(l, ENTITY_METATABLE);
        *p_entity = entity;
    }
    else
        lua_pushnil(l);

    return 1;
}

static int level_set_dude(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    int i = luaL_checkint(l, 2) - 1;
    luaL_argcheck(
        l,
        i >= 0 && i < L3_DUDE_COUNT,
        2,
        "dude index out of bounds"
    );
    b3_entity_id id = (b3_entity_id)luaL_checkunsigned(l, 3);

    level->dude_ids[i] = id;

    lua_pushvalue(l, 1);
    return 1;
}

static int entity_get_id(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    b3_entity_id id = b3_get_entity_id(entity);
    lua_pushunsigned(l, (lua_Unsigned)id);
    return 1;
}

static int entity_get_context(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    const struct entity_data *entity_data = b3_get_entity_data(entity);
    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    return 1;
}

static int entity_set_context(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    ref_entity_context(l, b3_get_entity_data(entity), 2);

    lua_pushvalue(l, 1);
    return 1;
}

static int entity_get_life(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    int life = b3_get_entity_life(entity);
    lua_pushinteger(l, (lua_Integer)life);
    return 1;
}

static int entity_set_life(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    int life = luaL_checkint(l, 2);

    b3_set_entity_life(entity, life);

    lua_pushvalue(l, 1);
    return 1;
}

static int entity_set_dirty(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    b3_set_entity_dirty(entity, 1);

    lua_pushvalue(l, 1);
    return 1;
}

static int open_level(lua_State *restrict l) {
    static const luaL_Reg functions[] = {
        {"new", level_new},
        {NULL, NULL}
    };
    static const luaL_Reg level_methods[] = {
        {"get_size", level_get_size},
        {"get_tile", level_get_tile},
        {"set_tile", level_set_tile},
        {"new_sprite", level_new_sprite},
        {"new_entity", level_new_entity},
        {"get_entity", level_get_entity},
        {"set_dude", level_set_dude},
        {NULL, NULL}
    };
    static const luaL_Reg sprite_methods[] = {
        {"destroy", sprite_destroy},
        {"get_pos", sprentity_get_pos},
        {"set_pos", sprentity_set_pos},
        {"set_image", sprentity_set_image},
        {"set_z_order", sprentity_set_z_order},
        {NULL, NULL}
    };
    static const luaL_Reg entity_methods[] = {
        {"get_id", entity_get_id},
        {"get_context", entity_get_context},
        {"set_context", entity_set_context},
        {"get_pos", sprentity_get_pos},
        {"set_pos", sprentity_set_pos},
        {"get_life", entity_get_life},
        {"set_life", entity_set_life},
        {"set_dirty", entity_set_dirty},
        {"set_image", sprentity_set_image},
        {"set_z_order", sprentity_set_z_order},
        {NULL, NULL}
    };

    luaL_newmetatable(l, LEVEL_METATABLE);

    lua_pushcfunction(l, level_gc);
    lua_setfield(l, -2, "__gc");
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, level_methods, 0);
    lua_pop(l, 1);

    luaL_newmetatable(l, SPRITE_METATABLE);

    // No __gc; rely on sprites going to 0 life to actually release them.
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, sprite_methods, 0);
    lua_pop(l, 1);

    luaL_newmetatable(l, ENTITY_METATABLE);

    // No __gc; rely on entities going to 0 life to actually release them.
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, entity_methods, 0);
    lua_pop(l, 1);

    luaL_newlib(l, functions);
    return 1;
}

static int open_all(lua_State *restrict l) {
    static const luaL_Reg submodules[] = {
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
    char *filename = b3_copy_format("%s/game/%s.lua", resource_path, base);
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
    _Bool client_,
    _Bool debug_
) {
    resource_path = b3_copy_string(resource_path_);
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

    b3_for_each_entity(level->entities, clean_entity, NULL);
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

char *l3_serialize_entity(b3_entity *restrict entity, size_t *restrict len) {
    const struct entity_data *entity_data = b3_get_entity_data(entity);
    lua_State *l = entity_data->l;

    if(entity_data->context_ref < 0 || !b3_get_entity_life(entity))
        return NULL;

    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    lua_getfield(l, -1, L3_ENTITY_SERIALIZE_NAME);
    if(!lua_isfunction(l, -1)) {
        lua_pop(l, 2);
        return NULL;
    }

    lua_insert(l, -2);

    lua_call(l, 1, 1);

    size_t serial_len = 0;
    const char *serial = lua_tolstring(l, -1, &serial_len);
    char *r = NULL;
    if(serial)
        r = b3_alloc_copy(serial, serial_len + 1);

    lua_pop(l, 1);

    if(len)
        *len = serial_len;
    return r;
}

// This is unfortunate, but because the level comes from C in this case, we
// need a way to push it to Lua where it retains the same value in Lua each
// time.
void l3_set_sync_level(l3_level *restrict level) {
    luaL_unref(lua, LUA_REGISTRYINDEX, sync_level_ref);

    l3_level *lua_level = push_new_level(lua);
    l3_copy_level(lua_level, level);
    sync_level_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
}

static l3_level *push_sync_level(lua_State *restrict l) {
    lua_rawgeti(l, LUA_REGISTRYINDEX, sync_level_ref);
    l3_level *level = luaL_testudata(l, -1, LEVEL_METATABLE);
    if(!level)
        b3_fatal("Sync level not set");
    return level;
}

l3_level l3_get_sync_level(void) {
    l3_level copy;
    l3_copy_level(&copy, push_sync_level(lua));
    lua_pop(lua, 1);

    return copy;
}

void l3_sync_entity(
    b3_entity_id id,
    const b3_pos *restrict pos,
    int life,
    const char *restrict serial,
    size_t serial_len
) {
    lua_getglobal(lua, L3_SYNC_NAME);
    if(!lua_isfunction(lua, -1))
        b3_fatal("Missing global function %s", L3_SYNC_NAME);

    l3_level *level = push_sync_level(lua);
    b3_entity *entity = b3_get_entity(level->entities, id);

    if(entity) {
        const struct entity_data *entity_data = b3_get_entity_data(entity);
        lua_rawgeti(lua, LUA_REGISTRYINDEX, entity_data->entity_ref);
    }
    else {
        b3_entity **p_entity = push_new_entity(lua);
        *p_entity = b3_claim_entity(level->entities, id, free_entity_data);
        b3_set_entity_data(*p_entity, new_entity_data(lua, -1, level->map));
        entity = *p_entity;
    }

    b3_set_entity_pos(entity, pos);
    b3_set_entity_life(entity, life);

    lua_pushlstring(lua, serial, serial_len);

    lua_call(lua, 3, 0);
}

void l3_sync_deleted(b3_entity_id ids[], int count) {
    lua_getglobal(lua, L3_SYNC_DELETED_NAME);
    if(!lua_isfunction(lua, -1))
        b3_fatal("Missing global function %s", L3_SYNC_DELETED_NAME);

    push_sync_level(lua);
    lua_createtable(lua, count, 0);
    for(int i = 0; i < count; i++) {
        lua_pushinteger(lua, (lua_Integer)(i + 1));
        lua_pushunsigned(lua, (lua_Unsigned)ids[i]);
        lua_settable(lua, -3);
    }

    lua_call(lua, 2, 0);
}

l3_agent *l3_new_agent(l3_level *restrict level, int dude_index) {
    l3_agent *agent = b3_malloc(sizeof(*agent), 1);
    agent->entities = b3_ref_entity_pool(level->entities);
    agent->dude_id = level->dude_ids[dude_index];
    agent->first = 1;
    agent->thread = lua_newthread(lua);
    agent->thread_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
    return l3_ref_agent(agent);
}

l3_agent *l3_ref_agent(l3_agent *restrict agent) {
    agent->ref_count++;
    return agent;
}

void l3_free_agent(l3_agent *restrict agent) {
    if(agent && !--agent->ref_count) {
        b3_free_entity_pool(agent->entities);
        luaL_unref(lua, LUA_REGISTRYINDEX, agent->thread_ref);
        b3_free(agent, sizeof(*agent));
    }
}

// elapsed is only read on the first call to this function.  It's assumed to be
// the same for each subsequent call.
_Bool l3_think_agent(l3_agent *restrict agent, b3_ticks elapsed) {
    if(agent->done)
        return 1;

    b3_entity *entity = b3_get_entity(agent->entities, agent->dude_id);
    if(!entity || !b3_get_entity_life(entity))
        goto done;

    const struct entity_data *entity_data = b3_get_entity_data(entity);
    if(entity_data->context_ref < 0)
        goto done;

    lua_State *l = entity_data->l;
    lua_State *thread = agent->thread;

    int arg_count = 0;
    if(agent->first) {
        agent->first = 0;

        lua_rawgeti(thread, LUA_REGISTRYINDEX, entity_data->context_ref);
        lua_getfield(thread, -1, L3_ENTITY_THINK_AGENT_NAME);
        if(!lua_isfunction(thread, -1)) {
            lua_pop(thread, 2);
            goto done;
        }

        lua_insert(thread, -2);
        lua_rawgeti(thread, LUA_REGISTRYINDEX, entity_data->entity_ref);
        lua_pushnumber(thread, (lua_Number)b3_ticks_to_secs(elapsed));
        arg_count = 3;
    }

    int result = lua_resume(thread, l, arg_count);

    if(result != LUA_OK && result != LUA_YIELD) {
        lua_error(thread);
        goto done;
    }

    lua_settop(thread, 0);
    if(result == LUA_OK)
        goto done;

    return 0;

done:
    agent->done = 1;
    return 1;
}
