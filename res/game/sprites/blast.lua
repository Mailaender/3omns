local util   = require("util")
local obj    = require("object")
local Sprite = require("sprites.sprite")


local Blast = obj.class(Sprite)

Blast.TIME = 1

Blast.ANIMATION = {}
-- TODO: run this inside of a "module init" function?
local blast_frame_duration = Blast.TIME / #IMAGES.BLASTS
for i, v in ipairs(IMAGES.BLASTS) do
  Blast.ANIMATION[i] = {
    time = Blast.TIME - (i - 1) * blast_frame_duration,
    image = v,
    z_order = #IMAGES.BLASTS - i,
  }
end

function Blast:init(level, pos)
  self.time = Blast.TIME

  Sprite.init(self, level, pos, IMAGES.BLASTS[1], #IMAGES.BLASTS - 1)
end

function Blast:animate(backing, old_time)
  util.animate_sprentity(self, backing, self.time, old_time, Blast.ANIMATION)
end

function Blast:l3_update(backing, elapsed)
  local old_time = self.time

  self.time = self.time - elapsed
  if self.time <= 0 then
    self:destroy(backing)
    return
  end

  self:animate(backing, old_time)
end


return Blast
