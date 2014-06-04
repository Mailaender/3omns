/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
    l3 - Lua interface library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
*/

#include "internal.h"
#include "b3/b3.h"

#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>


void push_pos(lua_State *restrict l, const b3_pos *restrict pos) {
    lua_createtable(l, 0, 2);
    lua_pushinteger(l, (lua_Integer)(pos->x + 1));
    lua_setfield(l, -2, "x");
    lua_pushinteger(l, (lua_Integer)(pos->y + 1));
    lua_setfield(l, -2, "y");
}

b3_pos check_pos(lua_State *restrict l, int arg_index) {
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

void push_size(lua_State *restrict l, const b3_size *restrict size) {
    lua_createtable(l, 0, 2);
    lua_pushinteger(l, (lua_Integer)size->width);
    lua_setfield(l, -2, "width");
    lua_pushinteger(l, (lua_Integer)size->height);
    lua_setfield(l, -2, "height");
}

b3_size check_size(lua_State *restrict l, int arg_index) {
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

b3_rect check_rect(lua_State *restrict l, int arg_index) {
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
