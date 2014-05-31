local clone = {}
package.loaded[...] = clone

local serial   = require("serial")
local obj      = require("object")
local Entities = require("entities")


-- TODO: some way of "versioning" these, so people have to be playing with
-- similar, if not identical, code.  This might be a good idea for the Lua
-- files themselves, not just for the data string.

function clone.serialize(entity)
  return serial.serialize_type(entity:get_type()) .. entity:serialize()
end

local entities_cache = {}

local function get_entities(level)
  if not entities_cache[level] then
    entities_cache = {}
    entities_cache[level] = Entities(level)
  end
  return entities_cache[level]
end

function clone.clone(level, id, pos, life, serialized)
  local entities = get_entities(level)

  local type, start = serial.deserialize_type(serialized)
  assert(type, "Invalid serialized string")

  local entity = obj.create(type)
  entity:init_clone(entities, id, pos, life, serialized, start)
end
