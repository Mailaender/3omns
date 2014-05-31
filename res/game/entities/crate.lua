local serial = require("serial")
local obj    = require("object")
local Entity = require("entities.entity")


local Crate = obj.class("Crate", Entity)

function Crate:init(entities, pos)
  local backing = Entity.init(self, entities, nil, pos, 1)

  -- "Held" in the sense that when the crate is destroyed, an instance of the
  -- given entity type is called with this crate's pos in the constructor and
  -- nothing after it.
  self.held_type = nil

  self:set_visual(backing)
end

function Crate:init_clone(entities, id, pos, life, serialized, start)
  local backing = Entity.init(self, entities, id, pos, life)

  self:sync(serialized, start)

  self:set_visual(backing)
end

function Crate:serialize()
  return serial.serialize_type(self.held_type)
end

function Crate:sync(serialized, start)
  self.held_type, start = serial.deserialize_type(serialized, start)
  return start
end

function Crate:set_visual(backing)
  self:set_image(IMAGES.CRATE, backing)
  self:set_z_order(0, backing)
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


return Crate
