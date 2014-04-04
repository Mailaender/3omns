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

static l3_level *check_level(lua_State *restrict l, int index) {
    return luaL_checkudata(l, index, LEVEL_METATABLE);
}

static int level_new(lua_State *restrict l) {
    int width = (int)luaL_checkinteger(l, 1);
    int height = (int)luaL_checkinteger(l, 2);
    int max_entities = (int)luaL_checkinteger(l, 3);

    l3_level *level = lua_newuserdata(l, sizeof(*level));
    luaL_getmetatable(l, LEVEL_METATABLE);
    lua_setmetatable(l, -2);

    level->map = b3_new_map(&(b3_size){width, height});
    level->entities = b3_new_entity_pool(max_entities, level->map);
    return 1;
}

static int level_gc(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    l3_free_level(level);
    return 0;
}

static int level_size(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);

    b3_size size = b3_get_map_size(level->map);
    lua_pushinteger(l, (lua_Integer)size.width);
    lua_pushinteger(l, (lua_Integer)size.height);
    return 2;
}

static int level_get_tile(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    int x = (int)luaL_checkinteger(l, 2) - 1;
    int y = (int)luaL_checkinteger(l, 3) - 1;

    b3_size size = b3_get_map_size(level->map);
    luaL_argcheck(l, x >= 0 && x < size.width, 2, "x must satisfy 1 <= x <= level:size() width");
    luaL_argcheck(l, y >= 0 && y < size.height, 3, "y must satisfy 1 <= y <= level:size() height");

    lua_pushunsigned(
        l,
        (lua_Unsigned)b3_get_map_tile(level->map, &(b3_pos){x, y})
    );
    return 1;
}

static int level_set_tile(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    int x = (int)luaL_checkinteger(l, 2) - 1;
    int y = (int)luaL_checkinteger(l, 3) - 1;
    b3_tile tile = (b3_tile)luaL_checkunsigned(l, 4);

    b3_size size = b3_get_map_size(level->map);
    luaL_argcheck(l, x >= 0 && x < size.width, 2, "x must satisfy 1 <= x <= level:size() width");
    luaL_argcheck(l, y >= 0 && y < size.height, 3, "y must satisfy 1 <= y <= level:size() height");

    b3_set_map_tile(level->map, &(b3_pos){x, y}, tile);

    lua_pushvalue(l, 1);
    return 1;
}

static int open_level(lua_State *restrict l) {
    static const luaL_Reg functions[] = {
        {"new", level_new},
        {NULL, NULL}
    };
    static const luaL_Reg methods[] = {
        {"size",level_size},
        {"get_tile", level_get_tile},
        {"set_tile", level_set_tile},
        {NULL, NULL}
    };

    luaL_newmetatable(l, LEVEL_METATABLE);

    lua_pushcfunction(l, level_gc);
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
