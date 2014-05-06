local obj    = require("object")
local Entity = require("entities.entity")


local Crate = obj.class(Entity)

function Crate:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.CRATE, 0)

  self.killed = nil
end

-- "Hold" in the sense that when the crate is destroyed, the function gets
-- called, presumably to spawn something else.
function Crate:hold(f)
  self.killed = f
end

function Crate:holds()
  return self.killed
end

function Crate:bumped(dude, dude_backing)
  self:kill()
  return dude:is_super()
end


return Crate
