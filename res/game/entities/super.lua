local obj    = require("object")
local Entity = require("entities.entity")

local Super = obj.class("Super", Entity)
package.loaded[...] = Super


function Super:init_base(entities, backing)
  Entity.init_base(self, entities, backing)

  self.solid = false

  self:set_image(IMAGES.SUPER, backing)
  self:set_z_order(0, backing)
end

function Super:init(entities, pos)
  Entity.init(self, entities, pos, 1)
end

function Super:serialize()
  return ""
end

function Super:sync(serialized, start, backing)
  return start
end

function Super:bumped(dude, dude_backing)
  self:kill()
  dude:superify(dude_backing)
  return true
end
