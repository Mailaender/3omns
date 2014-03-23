#include "l3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>


static lua_State *lua = NULL;


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
