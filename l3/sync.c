/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
    l3 - Lua interface library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

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

#include "l3.h"
#include "internal.h"
#include "b3/b3.h"

#include <stddef.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>


int sync_level_ref = LUA_NOREF;


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
