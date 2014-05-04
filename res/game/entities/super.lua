local obj    = require("object")
local Entity = require("entities.entity")


local Super = obj.class(Entity)

function Super:init(entities, pos)
  self.solid = false

  Entity.init(self, entities, pos, 1, IMAGES.SUPER, 0)
end

function Super:bumped(dude, dude_backing)
  self:kill()
  dude:superify(dude_backing)
  return true
end


return Super
