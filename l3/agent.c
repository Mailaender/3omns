/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
    l3 - Lua interface library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
*/

#include "l3.h"
#include "internal.h"
#include "b3/b3.h"

#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>


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

// elapsed is only read on the first call to this function.  It's assumed to be
// the same for each subsequent call.
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
        lua_rawgeti(thread, LUA_REGISTRYINDEX, entity_data->entity_ref);
        lua_pushnumber(thread, (lua_Number)b3_ticks_to_secs(elapsed));
        arg_count = 3;
    }

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
