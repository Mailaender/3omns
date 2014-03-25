#include "l3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <lua5.2/lualib.h>


#define L3_NAME "l3"
#define IMAGE_NAME "image"

#define IMAGE_METATABLE L3_NAME "." IMAGE_NAME


static lua_State *lua = NULL;


static b3_rect get_rect(lua_State *restrict l, int index) {
    lua_getfield(l, index, "x");
    lua_getfield(l, index, "y");
    lua_getfield(l, index, "width");
    lua_getfield(l, index, "height");

    int x_is_num, y_is_num, width_is_num, height_is_num;
    int x = (int)lua_tointegerx(l, -4, &x_is_num);
    int y = (int)lua_tointegerx(l, -3, &y_is_num);
    int width = (int)lua_tointegerx(l, -2, &width_is_num);
    int height = (int)lua_tointegerx(l, -1, &height_is_num);

    if(!x_is_num || !y_is_num || !width_is_num || !height_is_num)
        luaL_error(l, "rect requires integer x, y, width, and height");

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
    luaL_checktype(l, 2, LUA_TTABLE);
    b3_rect rect = get_rect(l, 2);

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

static int open_all(lua_State *restrict l) {
    static const luaL_Reg submodules[] = {
        {IMAGE_NAME, open_image},
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
    lua_State *lua = lua_newstate(lalloc, NULL);
    if(!lua)
        b3_fatal("Error creating lua context");

    lua_atpanic(lua, panic);
    luaL_openlibs(lua);

    luaopen_l3(lua);
    lua_pop(lua, 1);

    return lua;
}

void l3_init(void) {
    lua = new_lua();
}

void l3_quit(void) {
    if(lua) {
        lua_close(lua);
        lua = NULL;
    }
}
