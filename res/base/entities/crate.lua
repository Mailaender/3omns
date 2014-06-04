--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
  base - base game logic for 3omns
  Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
]]

local obj    = require("object")
local Entity = require("entities.entity")

local Crate = obj.class("Crate", Entity)
package.loaded[...] = Crate

local serial = require("serial")


function Crate:init_base(entities, backing)
  Entity.init_base(self, entities, backing)

  -- "Held" in the sense that when the crate is destroyed, an instance of the
  -- given entity type is called with this crate's pos in the constructor and
  -- nothing after it.
  self.held_type = nil

  self:set_image(IMAGES.CRATE, backing)
  self:set_z_order(0, backing)
end

function Crate:init(entities, pos, held_type)
  Entity.init(self, entities, pos, 1)

  self.held_type = held_type
end

function Crate:serialize()
  return serial.serialize_type(self.held_type)
end

function Crate:sync(serialized, start, backing)
  self.held_type, start = serial.deserialize_type(serialized, start)
  return start
end

function Crate:hold(type)
  self.held_type = type

  self:set_dirty()
end

function Crate:holds()
  return self.held_type
end

function Crate:killed()
  if self.held_type then
    self.held_type(self.entities, self.pos)
  end
end

function Crate:bumped(dude, dude_backing)
  self:kill()
  return dude:is_super()
end
