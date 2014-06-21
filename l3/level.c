/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
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
#include <lua.h>
#include <lauxlib.h>


static l3_level *check_level(lua_State *restrict l, int index) {
    return luaL_checkudata(l, index, LEVEL_METATABLE);
}

l3_level *push_new_level(lua_State *restrict l) {
    l3_level *level = lua_newuserdata(l, sizeof(*level));
    luaL_setmetatable(l, LEVEL_METATABLE);

    return level;
}

static int level_new(lua_State *restrict l) {
    b3_size size = check_size(l, 1);
    int max_entities = luaL_checkint(l, 2);

    l3_level *level = push_new_level(l);

    l3_init_level(level, client, &size, max_entities);
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

struct entity_data *new_entity_data(
    lua_State *restrict l,
    int entity_index,
    b3_map *restrict map
) {
    struct entity_data *entity_data = b3_malloc(sizeof(*entity_data), 1);
    entity_data->l = l;

    // We're using the Lua state-global registry to keep references to the
    // entity userdata and its context (the Lua object passed to
    // l3.level.new_sprite/_entity).  There may be a more elegant way to go
    // about this, but this seemed pretty simple and I couldn't see any major
    // problems, so I went with it.
    lua_pushvalue(l, entity_index);
    entity_data->entity_ref = luaL_ref(l, LUA_REGISTRYINDEX);
    entity_data->context_ref = LUA_NOREF;

    entity_data->map = b3_ref_map(map);
    return entity_data;
}

static struct entity_data *ref_entity_context(
    lua_State *restrict l,
    struct entity_data *entity_data,
    int context_index
) {
    luaL_unref(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    entity_data->context_ref = LUA_NOREF;

    lua_pushvalue(l, context_index);
    if(lua_istable(l, -1))
        entity_data->context_ref = luaL_ref(l, LUA_REGISTRYINDEX);
    else
        lua_pop(l, 1);

    return entity_data;
}

void free_entity_data(b3_entity *restrict entity, void *entity_data) {
    struct entity_data *restrict d = entity_data;
    if(d) {
        luaL_unref(d->l, LUA_REGISTRYINDEX, d->entity_ref);
        luaL_unref(d->l, LUA_REGISTRYINDEX, d->context_ref);
        b3_free_map(d->map);
        b3_free(d, sizeof(*d));
    }
}

static b3_entity *check_sprite(lua_State *restrict l, int index) {
    return *(b3_entity **)luaL_checkudata(l, index, SPRITE_METATABLE);
}

static int level_new_sprite(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    // Index 2 is the Lua sprite object, passed to ref_entity_context() below.

    // "Sprites" are actually entities, but that don't have life in Lua, nor
    // are they sync'd to the server.
    b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
    luaL_setmetatable(l, SPRITE_METATABLE);

    *p_entity = b3_claim_entity(level->sprites, 0, free_entity_data);
    b3_set_entity_life(*p_entity, 1);
    struct entity_data *entity_data = new_entity_data(l, -1, level->map);
    b3_set_entity_data(*p_entity, ref_entity_context(l, entity_data, 2));
    return 1;
}

static int sprite_destroy(lua_State *restrict l) {
    b3_entity *entity = check_sprite(l, 1);

    b3_set_entity_life(entity, 0);

    return 0;
}

// Works on either "sprites" or entities.
static b3_entity *check_sprentity(lua_State *restrict l, int index) {
    b3_entity **p_entity = luaL_testudata(l, index, SPRITE_METATABLE);
    if(p_entity)
        return *p_entity;
    p_entity = luaL_testudata(l, index, ENTITY_METATABLE);
    if(p_entity)
        return *p_entity;

    const char *message = lua_pushfstring(
        l,
        "sprite or entity expected, got %s",
        luaL_typename(l, index)
    );
    luaL_argerror(l, index, message);
    return NULL;
}

// Works on either "sprites" or entities.
static int sprentity_get_pos(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);

    b3_pos pos = b3_get_entity_pos(entity);
    push_pos(l, &pos);
    return 1;
}

// Works on either "sprites" or entities.
static int sprentity_set_pos(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);
    struct entity_data *entity_data = b3_get_entity_data(entity);
    b3_pos pos = check_map_pos(l, 2, entity_data->map);

    b3_set_entity_pos(entity, &pos);

    lua_pushvalue(l, 1);
    return 1;
}

// Works on either "sprites" or entities.
static int sprentity_set_image(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);
    luaL_checkany(l, 2);
    b3_image *image = NULL;
    if(!lua_isnil(l, 2))
        image = check_image(l, 2);

    b3_set_entity_image(entity, image);

    lua_pushvalue(l, 1);
    return 1;
}

// Works on either "sprites" or entities.
static int sprentity_set_z_order(lua_State *restrict l) {
    b3_entity *entity = check_sprentity(l, 1);
    int z_order = luaL_checkint(l, 2);

    b3_set_entity_z_order(entity, z_order);

    lua_pushvalue(l, 1);
    return 1;
}

static b3_entity *check_entity(lua_State *restrict l, int index) {
    return *(b3_entity **)luaL_checkudata(l, index, ENTITY_METATABLE);
}

b3_entity **push_new_entity(lua_State *restrict l) {
    b3_entity **p_entity = lua_newuserdata(l, sizeof(*p_entity));
    luaL_setmetatable(l, ENTITY_METATABLE);

    return p_entity;
}

static int level_new_entity(lua_State *restrict l) {
    l3_level *level = check_level(l, 1);
    // Index 2 is the Lua entity object, passed to ref_entity_context() below.

    b3_entity **p_entity = push_new_entity(l);
    *p_entity = b3_claim_entity(level->entities, 0, free_entity_data);
    struct entity_data *entity_data = new_entity_data(l, -1, level->map);
    b3_set_entity_data(*p_entity, ref_entity_context(l, entity_data, 2));
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
    luaL_argcheck(
        l,
        i >= 0 && i < L3_DUDE_COUNT,
        2,
        "dude index out of bounds"
    );
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

static int entity_get_context(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    const struct entity_data *entity_data = b3_get_entity_data(entity);
    lua_rawgeti(l, LUA_REGISTRYINDEX, entity_data->context_ref);
    return 1;
}

static int entity_set_context(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);
    ref_entity_context(l, b3_get_entity_data(entity), 2);

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

static int entity_set_dirty(lua_State *restrict l) {
    b3_entity *entity = check_entity(l, 1);

    b3_set_entity_dirty(entity, 1);

    lua_pushvalue(l, 1);
    return 1;
}

int open_level(lua_State *restrict l) {
    const luaL_Reg functions[] = {
        {"new", level_new},
        {NULL, NULL}
    };
    const luaL_Reg level_methods[] = {
        {"get_size", level_get_size},
        {"get_tile", level_get_tile},
        {"set_tile", level_set_tile},
        {"new_sprite", level_new_sprite},
        {"new_entity", level_new_entity},
        {"get_entity", level_get_entity},
        {"set_dude", level_set_dude},
        {NULL, NULL}
    };
    const luaL_Reg sprite_methods[] = {
        {"destroy", sprite_destroy},
        {"get_pos", sprentity_get_pos},
        {"set_pos", sprentity_set_pos},
        {"set_image", sprentity_set_image},
        {"set_z_order", sprentity_set_z_order},
        {NULL, NULL}
    };
    const luaL_Reg entity_methods[] = {
        {"get_id", entity_get_id},
        {"get_context", entity_get_context},
        {"set_context", entity_set_context},
        {"get_pos", sprentity_get_pos},
        {"set_pos", sprentity_set_pos},
        {"get_life", entity_get_life},
        {"set_life", entity_set_life},
        {"set_dirty", entity_set_dirty},
        {"set_image", sprentity_set_image},
        {"set_z_order", sprentity_set_z_order},
        {NULL, NULL}
    };

    luaL_newmetatable(l, LEVEL_METATABLE);

    lua_pushcfunction(l, level_gc);
    lua_setfield(l, -2, "__gc");
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, level_methods, 0);
    lua_pop(l, 1);

    luaL_newmetatable(l, SPRITE_METATABLE);

    // No __gc; rely on sprites going to 0 life to actually release them.
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    luaL_setfuncs(l, sprite_methods, 0);
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
