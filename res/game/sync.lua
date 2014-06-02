local sync = {}
package.loaded[...] = sync

local serial   = require("serial")
local obj      = require("object")
local Entities = require("entities")


-- TODO: some way of "versioning" these, so people have to be playing with
-- similar, if not identical, code.  This might be a good idea for the Lua
-- files themselves, not just for the data string.

function sync.serialize(entity)
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

function sync.sync(level, backing, serialized)
  local entities = get_entities(level)

  local type, start = serial.deserialize_type(serialized)
  assert(type, "Invalid serialized string")

  local entity = backing:get_context()
  if not entity then
    entity = obj.create(type)
    backing:set_context(entity)
  end

  entity:sync_base(entities, backing)
  entity:sync(serialized, start, backing)
end
