--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <http://chazomaticus.github.io/3omns/>
  base - base game logic for 3omns
  Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

  3omns is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  3omns is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  3omns.  If not, see <http://www.gnu.org/licenses/>.
]]

local sync = {}
package.loaded[...] = sync

local serial = require("serial")
local obj = require("object")
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

function sync.sync_deleted(level, deleted)
  local entities = get_entities(level)

  for _, id in ipairs(deleted) do
    local backing = entities:get_backing(id)
    if backing then
      entities:remove(backing:get_context())
    end
  end
end
