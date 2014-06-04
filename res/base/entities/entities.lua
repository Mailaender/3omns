--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
  base - base game logic for 3omns
  Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
]]

local obj = require("object")

local Entities = obj.class("Entities")
package.loaded[...] = Entities

local core = require("core")
local util = require("util")

local Super = require("entities.super")
local Crate = require("entities.crate")
local Bomn  = require("entities.bomn")
local Dude  = require("entities.dude")


Entities.Super = Super
Entities.Crate = Crate
Entities.Bomn  = Bomn
Entities.Dude  = Dude

function Entities:init(level)
  self.level = level
  self.level_size = level:get_size()
  self.type_index = {}
  -- Dudes don't go in the pos_index, because they can stand on top of certain
  -- entities, whereas everything else must uniquely occupy a position.
  self.pos_index = {}
end

-- The C entities are referred to as the "backing" for Lua entities.  Deal.
-- Backings should never be stored except as a temporary local.  If you store
-- the Lua entity, use Entity:exists() to double check the backing hasn't been
-- destroyed in C while your back was turned.
function Entities:new_backing(entity)
  assert(not l3.CLIENT, "Clients can't create new entities")

  return self.level:new_entity(entity)
end

function Entities:add(entity)
  -- It would be nice to handle *all* index entries we need here, e.g. what
  -- happens in set_dude() and move(), but this is called before any of the
  -- entity's fields are filled in, other than what's in the base Entity class.
  -- So, we have to provide some separate methods to call when everything's all
  -- filled out.

  local type = entity:get_type()
  if not self.type_index[type] then
    self.type_index[type] = {}
  end
  self.type_index[type][entity.id] = entity
end

function Entities:remove(entity)
  self.type_index[entity:get_type()][entity.id] = nil
  self.pos_index[core.pos_key(entity.pos)] = nil
end

function Entities:set_dude(dude)
  self.level:set_dude(dude.player, dude.id)
end

function Entities:get_backing(id)
  return self.level:get_entity(id)
end

function Entities:exists(id)
  return self:get_backing(id) ~= nil
end

function Entities:get_tile(pos)
  return self.level:get_tile(pos)
end

function Entities:pos_valid(pos)
  return pos.x >= 1 and pos.x <= self.level_size.width
      and pos.y >= 1 and pos.y <= self.level_size.height
end

function Entities:walkable(pos)
  return self:pos_valid(pos) and self:get_tile(pos) ~= TILES.WALL
end

function Entities:get_entity(pos)
  for _, d in pairs(self.type_index[Dude]) do
    if core.pos_equal(pos, d.pos) then return d end
  end

  return self.pos_index[core.pos_key(pos)]
end

function Entities:move(entity, old_pos)
  if entity:get_type() == Dude then return end

  if old_pos then
    self.pos_index[core.pos_key(old_pos)] = nil
  end

  local key = core.pos_key(entity.pos)
  assert(not self.pos_index[key], "Two entities can't occupy the same space")
  self.pos_index[key] = entity
end

function Entities:get_nearest(type, pos)
  local a = {}
  for _, e in pairs(self.type_index[type] or {}) do
    a[#a + 1] = {dist = core.walk_dist(pos, e.pos), entity = e}
  end
  table.sort(a, function(a, b) return a.dist < b.dist end)
  return a
end
