/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <https://chazomaticus.github.io/3omns/>
    l3 - Lua interface library for 3omns
    Copyright 2014-2016 Charles Lindsay <chaz@chazomatic.us>

    3omns is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    3omns is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with 3omns.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "b3/b3.h"
#include "internal.h"

#include <lauxlib.h>
#include <lua.h>
#include <stddef.h>


static b3_image **push_new_image(lua_State *restrict l) {
    b3_image **p_image = lua_newuserdata(l, sizeof(*p_image));
    luaL_setmetatable(l, IMAGE_METATABLE);

    return p_image;
}

b3_image *check_image(lua_State *restrict l, int index) {
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

int open_image(lua_State *restrict l) {
    const luaL_Reg functions[] = {
        {"load", image_load},
        {NULL, NULL}
    };
    const luaL_Reg methods[] = {
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
