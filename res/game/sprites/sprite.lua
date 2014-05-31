local core = require("core")
local obj  = require("object")


local Sprite = obj.class("Sprite")

function Sprite:init(level, pos)
  local backing = level:new_sprite(self)

  self:set_pos(pos, backing)

  return backing
end

Sprite.get_type = obj.get_type

function Sprite:set_pos(pos, backing)
  if self.pos == nil or not core.pos_equal(pos, self.pos) then
    self.pos = pos
    backing:set_pos(pos)
  end

  return self
end

function Sprite:set_image(image, backing)
  if image ~= self.image then
    self.image = image
    backing:set_image(image)
  end

  return self
end

function Sprite:set_z_order(z_order, backing)
  if z_order ~= self.z_order then
    self.z_order = z_order
    backing:set_z_order(z_order)
  end

  return self
end

function Sprite:destroy(backing)
  backing:destroy()
end


return Sprite
