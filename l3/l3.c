#include "l3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdio.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <lua5.2/lualib.h>


#define L3_NAME "l3"
#define IMAGE_NAME "image"
#define MAP_NAME "map"
#define ENTITY_NAME "entity"

#define IMAGE_METATABLE L3_NAME "." IMAGE_NAME
#define MAP_METATABLE L3_NAME "." MAP_NAME
#define ENTITY_METATABLE L3_NAME "." ENTITY_NAME

#define ENTITY_BACKING_FIELD "_b3_entity"


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
    return (b3_rect){x, y, width, height};
}

static b3_image **push_new_image(lua_State *restrict l) {
    b3_image **p_image = lua_newuserdata(l, sizeof(*p_image));
    luaL_getmetatable(l, IMAGE_METATABLE);
    lua_setmetatable(l, -2);

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

static b3_map *check_map(lua_State *restrict l, int index) {
    return *(b3_map **)luaL_checkudata(l, index, MAP_METATABLE);
}

static int map_new(lua_State *restrict l) {
    int width = (int)luaL_checkinteger(l, 1);
    int height = (int)luaL_checkinteger(l, 2);

    b3_map **p_map = lua_newuserdata(l, sizeof(*p_map));
    luaL_getmetatable(l, MAP_METATABLE);
    lua_setmetatable(l, -2);

    *p_map = b3_new_map(width, height);
    return 1;
}

static int map_gc(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);
    b3_free_map(map);
    return 0;
}

static int map_width(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);

    lua_pushinteger(l, (lua_Integer)b3_get_map_width(map));
    return 1;
}

static int map_height(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);

    lua_pushinteger(l, (lua_Integer)b3_get_map_height(map));
    return 1;
}

static int map_get_tile(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);
    int x = (int)luaL_checkinteger(l, 2) - 1;
    int y = (int)luaL_checkinteger(l, 3) - 1;

    luaL_argcheck(l, x >= 0 && x < b3_get_map_width(map), 2, "x must satisfy 1 <= x <= map:width()");
    luaL_argcheck(l, y >= 0 && y < b3_get_map_height(map), 3, "y must satisfy 1 <= y <= map:height()");

    lua_pushunsigned(l, (lua_Unsigned)b3_get_map_tile(map, x, y));
    return 1;
}

static int map_set_tile(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);
    int x = (int)luaL_checkinteger(l, 2) - 1;
    int y = (int)luaL_checkinteger(l, 3) - 1;
    b3_tile tile = (b3_tile)luaL_checkunsigned(l, 4);

    luaL_argcheck(l, x >= 0 && x < b3_get_map_width(map), 2, "x must satisfy 1 <= x <= map:width()");
    luaL_argcheck(l, y >= 0 && y < b3_get_map_height(map), 3, "y must satisfy 1 <= y <= map:height()");

    b3_set_map_tile(map, x, y, tile);

    lua_pushvalue(l, 1);
    return 1;
}

static int open_map(lua_State *restrict l) {
    static const luaL_Reg functions[] = {
        {"new", map_new},
        {NULL, NULL}
    };
    static const luaL_Reg methods[] = {
        {"width", map_width},
        {"height", map_height},
        {"get_tile", map_get_tile},
        {"set_tile", map_set_tile},
        {NULL, NULL}
    };

    luaL_newmetatable(l, MAP_METATABLE);

    lua_pushcfunction(l, map_gc);
    lua_setfield(l, -2, "__gc");
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, methods, 0);
    lua_pop(l, 1);

    luaL_newlib(l, functions);
    return 1;
}

static b3_entity *check_entity(lua_State *restrict l, int index) {
    return *(b3_entity **)luaL_checkudata(l, index, ENTITY_METATABLE);
}

static int entity_new(lua_State *restrict l) {
    lua_newtable(l);

    b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
    luaL_getmetatable(l, ENTITY_METATABLE);
    lua_setmetatable(l, -2);

    *p_entity = b3_new_entity();
    lua_setfield(l, -2, ENTITY_BACKING_FIELD);
    return 1;
}

static int entity_gc(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    b3_free_entity(entity);
    return 0;
}

static int open_entity(lua_State *restrict l) {
    static const luaL_Reg functions[] = {
        {"new", entity_new},
        {NULL, NULL}
    };

    luaL_newmetatable(l, ENTITY_METATABLE);

    lua_pushcfunction(l, entity_gc);
    lua_setfield(l, -2, "__gc");

    lua_pop(l, 1);

    luaL_newlib(l, functions);
    return 1;
}

static int open_all(lua_State *restrict l) {
    static const luaL_Reg submodules[] = {
        {IMAGE_NAME, open_image},
        {MAP_NAME, open_map},
        {ENTITY_NAME, open_entity},
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

b3_map *l3_generate_map(void) {
    lua_getglobal(lua, "generate_map");
    if(!lua_isfunction(lua, -1))
        b3_fatal("Missing global function generate_map");

    lua_call(lua, 0, 1); // Rely on panic to handle errors.

    b3_map **p_map = luaL_testudata(lua, -1, MAP_METATABLE);
    if(!p_map)
        b3_fatal("generate_map didn't return a map");
    b3_map *map = b3_ref_map(*p_map);
    lua_pop(lua, 1);

    return map;
}

// TODO: separate generate_entities function (make sure b3_entity params are
// set for everything when it finishes).

static void update_entity(lua_State *restrict l) {
    lua_getfield(l, -1, "run");
    lua_State *thread = lua_tothread(l, -1);
    if(thread) { // TODO: and thread hasn't exited yet?
        lua_resume(thread, l, 0);
        // TODO: then set b3_entity params from new values.
    }
    lua_pop(l, 1);
}

void l3_update_entities(b3_ticks elapsed) {
    lua_getglobal(lua, "ENTITIES");
    if(!lua_istable(lua, -1))
        b3_fatal("Missing global table ENTITIES");

    lua_pushnil(lua);
    while(lua_next(lua, -2)) {
        update_entity(lua);
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
}
