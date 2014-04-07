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

struct entity_data {
    lua_State *l;
    int entity_ref; // Lua reference to the entity userdata.
    int update_ref; // Lua reference to the context object or update function.
    b3_map *map;
};

struct update_entity_data {
    lua_Number elapsed;
};


b3_image *l3_border_image = NULL;
b3_image *l3_tile_images[B3_TILE_COUNT] = {NULL};

static char *resource_path = NULL;
static lua_State *lua = NULL;


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
        (x_is_num && y_is_num && width_is_num && height_is_num),
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
    int width = (int)luaL_checkinteger(l, 1);
    int height = (int)luaL_checkinteger(l, 2);
    int max_entities = (int)luaL_checkinteger(l, 3);

    l3_level *level = lua_newuserdata(l, sizeof(*level));
    luaL_setmetatable(l, LEVEL_METATABLE);

    level->map = b3_new_map(&(b3_size){width, height});
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
    lua_pushinteger(l, (lua_Integer)size.width);
    lua_pushinteger(l, (lua_Integer)size.height);
    return 2;
}

static void check_map_pos(
    lua_State *restrict l,
    b3_map *restrict map,
    int x_index,
    int y_index,
    b3_pos *restrict pos_out
) {
    int x = (int)luaL_checkinteger(l, x_index) - 1;
    int y = (int)luaL_checkinteger(l, y_index) - 1;

    b3_size size = b3_get_map_size(map);
    luaL_argcheck(
        l,
        x >= 0 && x < size.width,
        x_index,
        "x must satisfy 1 <= x <= level:get_size() width"
    );
    luaL_argcheck(
        l,
        y >= 0 && y < size.height,
        y_index,
        "y must satisfy 1 <= y <= level:get_size() height"
    );

    *pos_out = (b3_pos){x, y};
}

static int level_get_tile(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    b3_pos pos;
    check_map_pos(l, level->map, 2, 3, &pos);

    lua_pushunsigned(l, (lua_Unsigned)b3_get_map_tile(level->map, &pos));
    return 1;
}

static int level_set_tile(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    b3_pos pos;
    check_map_pos(l, level->map, 2, 3, &pos);
    b3_tile tile = (b3_tile)luaL_checkunsigned(l, 4);

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
    int update_index,
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
    entity_data->update_ref = LUA_NOREF;
    lua_pushvalue(l, update_index);
    if(lua_isfunction(l, -1) || lua_istable(l, -1)) {
        entity_data->update_ref = luaL_ref(l, LUA_REGISTRYINDEX);

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
        luaL_unref(d->l, LUA_REGISTRYINDEX, d->update_ref);
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

static int entity_gc(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    b3_release_entity(entity);
    return 0;
}

static int entity_set_pos(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    struct entity_data *entity_data = b3_get_entity_data(entity);
    b3_pos pos;
    check_map_pos(l, entity_data->map, 2, 3, &pos);

    b3_set_entity_pos(entity, &pos);

    lua_pushvalue(l, 1);
    return 1;
}

static int entity_set_life(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    int life = (int)luaL_checkinteger(l, 2);

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
        {NULL, NULL}
    };
    static const luaL_Reg entity_methods[] = {
        {"set_pos", entity_set_pos},
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

    lua_pushcfunction(l, entity_gc);
    lua_setfield(l, -2, "__gc");
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

static void set_border_image(lua_State *restrict l) {
    lua_getglobal(l, "IMAGES");
    if(!lua_istable(l, -1))
        b3_fatal("Missing global table IMAGES");

    lua_getfield(l, -1, "BORDER");
    b3_image **p_image = luaL_testudata(l, -1, IMAGE_METATABLE);
    if(!p_image)
        b3_fatal("Missing IMAGES.BORDER image");
    l3_border_image = b3_ref_image(*p_image);

    lua_pop(l, 2);
}

static void set_tile_images(lua_State *restrict l) {
    lua_getglobal(l, "TILE_IMAGES");
    if(!lua_istable(l, -1))
        b3_fatal("Missing global table TILE_IMAGES");

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

void l3_init(const char *restrict resource_path_) {
    resource_path = b3_copy_string(resource_path_);

    lua = new_lua();
    run_game_file(lua, "init");

    set_border_image(lua);
    set_tile_images(lua);
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
    lua_getglobal(lua, "generate");
    if(!lua_isfunction(lua, -1))
        b3_fatal("Missing global function generate");

    lua_call(lua, 0, 1); // Rely on panic to handle errors.

    l3_level *level = luaL_testudata(lua, -1, LEVEL_METATABLE);
    if(!level)
        b3_fatal("generate didn't return a level");
    l3_level copy = l3_copy_level(level);
    lua_pop(lua, 1);

    return copy;
}

// TODO: separate sprite object that gets updated (and only locally), but can't
// act or be acted on.

// TODO: separate AI/Agent object that has a thread/run coroutine that gets
// resumed 10 or so times per second, that controls a particular entity.

static void update_entity(
    b3_entity *restrict entity,
    void *entity_data,
    void *callback_data
) {
    const struct entity_data *restrict data = entity_data;
    const struct update_entity_data *restrict update_data = callback_data;

    if(data->update_ref < 0) // read: == LUA_NOREF || == LUA_REFNIL
        return;

    lua_State *l = data->l;

    lua_rawgeti(l, LUA_REGISTRYINDEX, data->update_ref);
    if(lua_isfunction(l, -1)) {
        lua_rawgeti(l, LUA_REGISTRYINDEX, data->entity_ref);
        lua_pushnumber(l, update_data->elapsed);

        lua_call(l, 2, 0);
        return;
    }

    lua_getfield(l, -1, "update");
    if(!lua_isfunction(l, -1)) {
        lua_pop(l, 2);
        return;
    }

    lua_rawgeti(l, LUA_REGISTRYINDEX, data->entity_ref);
    lua_pushvalue(l, -3); // data->update_ref again.
    lua_pushnumber(l, update_data->elapsed);

    lua_call(l, 3, 0);
    lua_pop(l, 1);
}

void l3_update(l3_level *restrict level, b3_ticks elapsed) {
    b3_for_each_entity(
        level->entities,
        update_entity,
        &(struct update_entity_data){
            (lua_Number)b3_ticks_to_secs(elapsed)
        }
    );
}
