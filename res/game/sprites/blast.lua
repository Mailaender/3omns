local obj    = require("object")
local Sprite = require("sprites.sprite")

local Blast = obj.class("Blast", Sprite)
package.loaded[...] = Blast

local util   = require("util")


Blast.TIME = 1

-- TODO: add some variance here, so the blasts aren't quite so uniform, but
-- speckle a bit as they dissipate.
local function get_animation()
  if not Blast.ANIMATION then
    Blast.ANIMATION = {}

    local blast_frame_duration = Blast.TIME / #IMAGES.BLASTS
    for i, v in ipairs(IMAGES.BLASTS) do
      Blast.ANIMATION[i] = {
        time = Blast.TIME - (i - 1) * blast_frame_duration,
        image = v,
        z_order = #IMAGES.BLASTS - i,
      }
    end
  end

  return Blast.ANIMATION
end

function Blast:init(level, pos)
  local backing = Sprite.init(self, level, pos)

  self.time = Blast.TIME

  self:animate(backing)
end

function Blast:animate(backing)
  local frame = util.get_animation_frame(get_animation(), self.time)
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
