#ifndef __internal_h__
#define __internal_h__

#include "l3.h"
#include "b3/b3.h"

#include <lua5.2/lua.h>


#define L3_NAME "l3"
#define IMAGE_NAME "image"
#define LEVEL_NAME "level"

#define IMAGE_METATABLE L3_NAME "." IMAGE_NAME
#define LEVEL_METATABLE L3_NAME "." LEVEL_NAME
#define SPRITE_METATABLE L3_NAME ".sprite"
#define ENTITY_METATABLE L3_NAME ".entity"

#define L3_GENERATE_NAME "l3_generate"
#define L3_SYNC_NAME "l3_sync"
#define L3_SYNC_DELETED_NAME "l3_sync_deleted"

#define L3_TILE_IMAGES_NAME "L3_TILE_IMAGES"
#define L3_BORDER_IMAGE_NAME "L3_BORDER_IMAGE"
#define L3_HEART_IMAGES_NAME "L3_HEART_IMAGES"

#define L3_ENTITY_UPDATE_NAME "l3_update"
#define L3_ENTITY_ACTION_NAME "l3_action"
#define L3_ENTITY_SERIALIZE_NAME "l3_serialize"
#define L3_ENTITY_THINK_AGENT_NAME "l3_think"


struct entity_data {
    lua_State *l;
    int entity_ref; // Lua reference to the entity userdata.
    int context_ref; // Lua reference to the context object.
    b3_map *map;
};


void push_pos(lua_State *restrict l, const b3_pos *restrict pos);
b3_pos check_pos(lua_State *restrict l, int arg_index);

void push_size(lua_State *restrict l, const b3_size *restrict size);
b3_size check_size(lua_State *restrict l, int arg_index);

b3_rect check_rect(lua_State *restrict l, int arg_index);

b3_image *check_image(lua_State *restrict l, int index);

l3_level *push_new_level(lua_State *restrict l);
b3_entity **push_new_entity(lua_State *restrict l);


struct entity_data *new_entity_data(
    lua_State *restrict l,
    int entity_index,
    b3_map *restrict map
);
void free_entity_data(b3_entity *restrict entity, void *entity_data);


int open_image(lua_State *restrict l);
int open_level(lua_State *restrict l);


extern char *resource_path;
extern _Bool client;
extern _Bool debug;
extern lua_State *lua;
extern int sync_level_ref;


#endif