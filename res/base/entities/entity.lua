--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
  base - base game logic for 3omns
  Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
]]

local obj = require("object")

local Entity = obj.class("Entity")
package.loaded[...] = Entity

local core = require("core")
local sync = require("sync")


function Entity:init_base(entities, backing)
  self.entities = entities
  self.id       = backing:get_id()
  self.solid    = true -- Whether the entity blocks blasts and player movement.

  entities:add(self)
end

function Entity:init(entities, pos, life)
  local backing = entities:new_backing(self)
  self:init_base(entities, backing)

  self:set_pos(pos, backing)
  self:set_life(life, backing)
end

function Entity:sync_base(entities, backing)
  if not self.id then
    self:init_base(entities, backing)
  end

  local pos = backing:get_pos()
  if self.pos == nil or not core.pos_equal(pos, self.pos) then
    local old = self.pos
    self.pos = pos
    self.entities:move(self, old)
  end
  self.life = backing:get_life()
end

Entity.l3_serialize = sync.serialize

Entity.get_type = obj.get_type

function Entity:get_backing()
  return self.entities:get_backing(self.id)
end

function Entity:exists()
  return self:get_backing() ~= nil
end

function Entity:get_nearest(type, pos)
  pos = pos or self.pos

  return self.entities:get_nearest(type, pos)
end

function Entity:set_pos(pos, backing)
  if not l3.CLIENT and (not self.pos or not core.pos_equal(pos, self.pos)) then
    backing = backing or self:get_backing()
    local old = self.pos

    self.pos = pos
    backing:set_pos(pos)

    self.entities:move(self, old)
  end

  return self
end

function Entity:set_life(life, backing)
  if not l3.CLIENT and life ~= self.life then
    backing = backing or self:get_backing()

    self.life = life
    backing:set_life(life)

    if life == 0 then
      self.entities:remove(self)
      if self.killed then
        self:killed()
      end
    end
  end

  return self
end

function Entity:set_dirty(backing)
  backing = backing or self:get_backing()

  backing:set_dirty()
  return self
end

function Entity:set_image(image, backing)
  if image ~= self.image then
    backing = backing or self:get_backing()

    self.image = image
    backing:set_image(image)
  end

  return self
end

function Entity:set_z_order(z_order, backing)
  if z_order ~= self.z_order then
    backing = backing or self:get_backing()

    self.z_order = z_order
    backing:set_z_order(z_order)
  end

  return self
end

function Entity:kill(backing)
  self:set_life(0, backing)
end

function Entity:hurt(damage, backing)
  -- TODO: keep track of who's doing the hurting.
  self:set_life(math.max(0, self.life - damage), backing)
end

function Entity:bumped(dude, dude_backing)
  return not self.solid
end

function Entity:blasted(bomn, bomn_backing)
  if self.solid then
    self:kill()
  end
end
