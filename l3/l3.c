#include "l3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <lua5.2/lualib.h>


#define L3_NAME "l3"
#define IMAGE_NAME "image"
#define MAP_NAME "map"

#define IMAGE_METATABLE L3_NAME "." IMAGE_NAME
#define MAP_METATABLE L3_NAME "." MAP_NAME


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

    lua_pushinteger(l, (lua_Integer)map->width);
    return 1;
}

static int map_height(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);

    lua_pushinteger(l, (lua_Integer)map->height);
    return 1;
}

static int map_get_tile(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);
    int x = (int)luaL_checkinteger(l, 2) - 1;
    int y = (int)luaL_checkinteger(l, 3) - 1;

    luaL_argcheck(l, x >= 0 && x < map->width, 2, "x must satisfy 1 <= x <= map:width()");
    luaL_argcheck(l, y >= 0 && y < map->height, 3, "y must satisfy 1 <= y <= map:height()");

    lua_pushinteger(l, (lua_Integer)B3_MAP_TILE(map, x, y));
    return 1;
}

static int map_set_tile(lua_State *restrict l) {
    b3_map *map = check_map(l, 1);
    int x = (int)luaL_checkinteger(l, 2) - 1;
    int y = (int)luaL_checkinteger(l, 3) - 1;
    b3_tile tile = (b3_tile)luaL_checkinteger(l, 4);

    luaL_argcheck(l, x >= 0 && x < map->width, 2, "x must satisfy 1 <= x <= map:width()");
    luaL_argcheck(l, y >= 0 && y < map->height, 3, "y must satisfy 1 <= y <= map:height()");

    B3_MAP_TILE(map, x, y) = tile;

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

static int open_all(lua_State *restrict l) {
    static const luaL_Reg submodules[] = {
        {IMAGE_NAME, open_image},
        {MAP_NAME, open_map},
        {NULL, NULL}
    };

    luaL_newlibtable(l, submodules);
    for(const luaL_Reg *submodule = submodules; submodule->func; submodule++) {
        submodule->func(l);
        lua_setfield(l, -2, submodule->name);
    }
    return 1;
}

int luaopen_l3(lua_State *restrict l) {
    luaL_requiref(l, L3_NAME, open_all, 1);
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

    luaopen_l3(l);
    lua_pop(l, 1);

    return l;
}

static void run(lua_State *restrict l, const char *restrict filename) {
    if(luaL_dofile(l, filename))
        b3_fatal("Error running Lua file %s: %s", filename, lua_tostring(l, -1));
}

void l3_init(const char *restrict init_filename) {
    lua = new_lua();
    run(lua, init_filename);
}

void l3_quit(void) {
    if(lua) {
        lua_close(lua);
        lua = NULL;
    }
}
