/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <https://chazomaticus.github.io/3omns/>
    l3 - Lua interface library for 3omns
    Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

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
#include "l3.h"

#include <lauxlib.h>
#include <lua.h>


struct l3_agent {
    int ref_count;
    b3_entity_pool *entities;
    b3_entity_id dude_id;
    _Bool first;
    _Bool done;
    lua_State *thread;
    int thread_ref;
};


l3_agent *l3_new_agent(l3_level *restrict level, int dude_index) {
    l3_agent *agent = b3_malloc(sizeof(*agent), 1);
    agent->entities = b3_ref_entity_pool(level->entities);
    agent->dude_id = level->dude_ids[dude_index];
    agent->first = 1;
    agent->thread = lua_newthread(lua);
    agent->thread_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
    return l3_ref_agent(agent);
}

l3_agent *l3_ref_agent(l3_agent *restrict agent) {
    agent->ref_count++;
    return agent;
}

void l3_free_agent(l3_agent *restrict agent) {
    if(agent && !--agent->ref_count) {
        b3_free_entity_pool(agent->entities);
        luaL_unref(lua, LUA_REGISTRYINDEX, agent->thread_ref);
        b3_free(agent, sizeof(*agent));
    }
}

_Bool l3_think_agent(l3_agent *restrict agent, b3_ticks elapsed) {
    if(agent->done)
        return 1;

    b3_entity *entity = b3_get_entity(agent->entities, agent->dude_id);
    if(!entity || !b3_get_entity_life(entity))
        goto done;

    const struct entity_data *entity_data = b3_get_entity_data(entity);
    if(entity_data->context_ref < 0)
        goto done;

    lua_State *l = entity_data->l;
    lua_State *thread = agent->thread;

    int arg_count = 0;
    if(agent->first) {
        agent->first = 0;

        lua_rawgeti(thread, LUA_REGISTRYINDEX, entity_data->context_ref);
        lua_getfield(thread, -1, L3_ENTITY_THINK_AGENT_NAME);
        if(!lua_isfunction(thread, -1)) {
            lua_pop(thread, 2);
            goto done;
        }

        lua_insert(thread, -2);
        arg_count++;
        lua_rawgeti(thread, LUA_REGISTRYINDEX, entity_data->entity_ref);
        arg_count++;
    }
    lua_pushnumber(thread, (lua_Number)b3_ticks_to_secs(elapsed));
    arg_count++;

    int result = lua_resume(thread, l, arg_count);

    if(result != LUA_OK && result != LUA_YIELD) {
        lua_error(thread);
        goto done;
    }

    lua_settop(thread, 0);
    if(result == LUA_OK)
        goto done;

    return 0;

done:
    agent->done = 1;
    return 1;
}
