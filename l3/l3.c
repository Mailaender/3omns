#include "l3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdio.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <lua5.2/lualib.h>


#define L3_NAME "l3"
#define IMAGE_NAME "image"
#define LEVEL_NAME "level"

#define IMAGE_METATABLE L3_NAME "." IMAGE_NAME
#define LEVEL_METATABLE L3_NAME "." LEVEL_NAME
#define ENTITY_METATABLE L3_NAME ".entity"

#define L3_GENERATE_NAME "l3_generate"
#define L3_TILE_IMAGES_NAME "L3_TILE_IMAGES"
#define L3_BORDER_IMAGE_NAME "L3_BORDER_IMAGE"

#define L3_ENTITY_UPDATE_NAME "l3_update"
#define L3_ENTITY_ACTION_NAME "l3_action"

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


b3_image *l3_border_image = NULL;
b3_image *l3_tile_images[B3_TILE_COUNT] = {NULL};

static char *resource_path = NULL;
static lua_State *lua = NULL;


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

static int level_new(lua_State *restrict l) {
    b3_size size = check_size(l, 1);
    int max_entities = luaL_checkint(l, 2);

    l3_level *level = lua_newuserdata(l, sizeof(*level));
    luaL_setmetatable(l, LEVEL_METATABLE);

    *level = (l3_level)L3_LEVEL_INIT;
    level->map = b3_new_map(&size);
    level->entities = b3_new_entity_pool(max_entities, level->map);
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

static b3_entity *check_entity(lua_State *restrict l, int index) {
    return *(b3_entity **)luaL_checkudata(l, index, ENTITY_METATABLE);
}

static struct entity_data *new_entity_data(
    lua_State *restrict l,
    int entity_index,
    int context_index,
    b3_map *restrict map
) {
    struct entity_data *entity_data = b3_malloc(sizeof(*entity_data), 1);
    entity_data->l = l;

    // We're using the Lua state-global registry to keep references to the
    // entity userdata and its context (the Lua object passed to
    // l3.level.new_entity).  There may be a more elegant way to go about this,
    // but this seemed pretty simple and I couldn't see any major problems, so
    // I went with it.
    entity_data->entity_ref = LUA_NOREF;
    entity_data->context_ref = LUA_NOREF;
    lua_pushvalue(l, context_index);
    if(lua_istable(l, -1)) {
        entity_data->context_ref = luaL_ref(l, LUA_REGISTRYINDEX);

        lua_pushvalue(l, entity_index);
        entity_data->entity_ref = luaL_ref(l, LUA_REGISTRYINDEX);
    }
    else
        lua_pop(l, 1);

    entity_data->map = b3_ref_map(map);
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

static int level_new_entity(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);

    b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
    luaL_setmetatable(l, ENTITY_METATABLE);

    *p_entity = b3_claim_entity(level->entities, free_entity_data);
    b3_set_entity_data(*p_entity, new_entity_data(l, -1, 2, level->map));
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
    luaL_argcheck(l, i >= 0 && i < L3_DUDE_COUNT, 2, "dude index out of bounds");
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

static int entity_get_pos(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    b3_pos pos = b3_get_entity_pos(entity);
    push_pos(l, &pos);
    return 1;
}

static int entity_set_pos(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    struct entity_data *entity_data = b3_get_entity_data(entity);
    b3_pos pos = check_map_pos(l, 2, entity_data->map);

    b3_set_entity_pos(entity, &pos);

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

static int entity_set_image(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    b3_image *image = check_image(l, 2);

    b3_set_entity_image(entity, image);

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
        {"new_entity", level_new_entity},
        {"get_entity", level_get_entity},
        {"set_dude", level_set_dude},
        {NULL, NULL}
    };
    static const luaL_Reg entity_methods[] = {
        {"get_id", entity_get_id},
        {"get_pos", entity_get_pos},
        {"set_pos", entity_set_pos},
        {"get_life", entity_get_life},
        {"set_life", entity_set_life},
        {"set_image", entity_set_image},
        {NULL, NULL}
    };

    luaL_newmetatable(l, LEVEL_METATABLE);

    lua_pushcfunction(l, level_gc);
    lua_setfield(l, -2, "__gc");
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, level_methods, 0);
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
    char f[1024];
    snprintf(f, sizeof(f), "%s/game/%s.lua", resource_path, base);
    f[sizeof(f) - 1] = '\0';

    if(luaL_dofile(l, f))
        b3_fatal("Error running game file %s: %s", base, lua_tostring(l, -1));
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

void l3_init(const char *restrict resource_path_) {
    resource_path = b3_copy_string(resource_path_);

    lua = new_lua();
    run_game_file(lua, "init");

    set_tile_images(lua);
    set_border_image(lua);
}

void l3_quit(void) {
    for(int i = 0; i < B3_TILE_COUNT; i++) {
        b3_free_image(l3_tile_images[i]);
        l3_tile_images[i] = NULL;
    }
    b3_free_image(l3_border_image);
    l3_border_image = NULL;
    if(lua) {
        lua_close(lua);
        lua = NULL;
    }
    b3_free(resource_path, 0);
    resource_path = NULL;
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
    l3_level copy = l3_copy_level(level);
    lua_pop(lua, 1);

    return copy;
}

// TODO: separate sprite object that gets updated (and only locally), but can't
// act or be acted on.

// TODO: separate AI/Agent object that has a thread/run coroutine that gets
// resumed 10 or so times per second, that controls a particular entity.

static void update_entity(b3_entity *restrict entity, void *callback_data) {
    const struct update_entity_data *restrict update_data = callback_data;

    const struct entity_data *restrict entity_data
            = b3_get_entity_data(entity);
    lua_State *restrict l = entity_data->l;

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

static void cull_entity(b3_entity *restrict entity, void *callback_data) {
    if(!b3_get_entity_life(entity))
        b3_release_entity(entity);
}

void l3_update(l3_level *restrict level, b3_ticks elapsed) {
    b3_for_each_entity(
        level->entities,
        update_entity,
        &(struct update_entity_data){
            (lua_Number)b3_ticks_to_secs(elapsed),
        }
    );

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
    const struct entity_data *restrict entity_data
            = b3_get_entity_data(entity);

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
