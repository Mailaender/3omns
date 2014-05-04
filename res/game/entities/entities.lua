local core = require("core")
local obj  = require("object")
local Dude = require("entities.dude")


local Entities = obj.class()

local public = {
  Super = require("entities.super"),
  Crate = require("entities.crate"),
  Bomn  = require("entities.bomn"),
  Dude  = Dude,
}

for name, constructor in pairs(public) do
  Entities[name] = constructor
end


function Entities:init(level)
  self.level = level
  self.level_size = level:get_size()
  self.index = {}
  self.dudes = {}
end

-- The C entities are referred to as the "backing" for Lua entities.  Deal.
-- Backings should never be stored except as a temporary local.  If you store
-- the Lua entity, use Entity:exists() to double check the backing hasn't been
-- destroyed in C while your back was turned.
function Entities:new_backing(entity)
  local backing = self.level:new_entity(entity)
  return backing:get_id(), backing
end

function Entities:get_backing(id)
  return self.level:get_entity(id)
end

function Entities:set_dude(dude)
  self.level:set_dude(dude.player, dude.id)
  self.dudes[dude.id] = dude
end

function Entities:get_tile(pos)
  return self.level:get_tile(pos)
end

function Entities:valid_pos(pos)
  return pos.x >= 1 and pos.x <= self.level_size.width
      and pos.y >= 1 and pos.y <= self.level_size.height
end

function Entities:walkable(pos)
  return self:valid_pos(pos) and self:get_tile(pos) ~= TILES.WALL
end

function Entities:get_entity(pos)
  for _, d in pairs(self.dudes) do
    if core.pos_equal(pos, d.pos) then return d end
  end

  return self.index[core.pos_key(pos)]
end

function Entities:move(entity, old_pos)
  if entity:is_a(Dude) then return end

  if old_pos then
    self.index[core.pos_key(old_pos)] = nil
  end

  local key = core.pos_key(entity.pos)
  assert(not self.index[key], "Two entities can't occupy the same space")
  self.index[key] = entity
end

function Entities:remove(entity)
  if entity:is_a(Dude) then
    self.dudes[entity.id] = nil
  else
    self.index[core.pos_key(entity.pos)] = nil
  end
end


return Entities
