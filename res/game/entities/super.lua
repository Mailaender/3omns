local obj    = require("object")
local Entity = require("entities.entity")

local Super = obj.class("Super", Entity)
package.loaded[...] = Super


function Super:init(entities, pos)
  local backing = Entity.init(self, entities, nil, pos, 1)

  self.solid = false

  self:set_visual(backing)
end

function Super:init_clone(entities, id, pos, life, serialized, start)
  local backing = Entity.init(self, entities, id, pos, life)

  self.solid = false -- TODO: avoid this duplication.
  self:sync(serialized, start)

  self:set_visual(backing)
end

function Super:serialize()
  return ""
end

function Super:sync(serialized, start)
  return start
end

function Super:set_visual(backing)
  self:set_image(IMAGES.SUPER, backing)
  self:set_z_order(0, backing)
  return self
end

function Super:bumped(dude, dude_backing)
  self:kill()
  dude:superify(dude_backing)
  return true
end
