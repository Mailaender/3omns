local serial = require("serial")
local obj    = require("object")
local Entity = require("entities.entity")


local Crate = obj.class(Entity)

function Crate:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.CRATE, 0)

  -- "Held" in the sense that when the crate is destroyed, an instance of the
  -- given entity type is called with this crate's pos in the constructor and
  -- nothing after it.
  self.held_type = nil
end

function Crate:serialize()
  return serial.serialize_type(self.held_type)
end

function Crate:deserialize(s, start)
  self.held_type, start = serial.deserialize_type(s, start)
  return start
end

function Crate:hold(type)
  self.held_type = type
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
