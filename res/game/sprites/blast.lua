local util   = require("util")
local obj    = require("object")
local Sprite = require("sprites.sprite")


local Blast = obj.class("Blast", Sprite)

Blast.TIME = 1

Blast.ANIMATION = {}
-- TODO: run this inside of a "module init" function?
-- TODO: add some variance here, so the blasts aren't quite so uniform, but
-- speckle a bit as they dissipate.
local blast_frame_duration = Blast.TIME / #IMAGES.BLASTS
for i, v in ipairs(IMAGES.BLASTS) do
  Blast.ANIMATION[i] = {
    time = Blast.TIME - (i - 1) * blast_frame_duration,
    image = v,
    z_order = #IMAGES.BLASTS - i,
  }
end

function Blast:init(level, pos)
  local backing = Sprite.init(self, level, pos)

  self.time = Blast.TIME

  self:animate(backing)
end

function Blast:animate(backing)
  local frame = util.get_animation_frame(Blast.ANIMATION, self.time)
  self:set_image(frame.image, backing)
  self:set_z_order(frame.z_order, backing)
end

function Blast:l3_update(backing, elapsed)
  self.time = self.time - elapsed
  if self.time <= 0 then
    self:destroy(backing)
    return
  end

  self:animate(backing)
end


return Blast
