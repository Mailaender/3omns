--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
  base - base game logic for 3omns
  Copyright 2014 Charles Lindsay <chaz@chazomatic.us>
]]

local obj    = require("object")
local Sprite = require("sprites.sprite")

local Blast = obj.class("Blast", Sprite)
package.loaded[...] = Blast

local util = require("util")


Blast.TIME = 1

function Blast:init_animation()
  self.animation = {}

  local frame_duration = Blast.TIME / #IMAGES.BLASTS
  for i, v in ipairs(IMAGES.BLASTS) do
    local a = {
      time = Blast.TIME - (i - 1) * frame_duration,
      image = v,
      z_order = #IMAGES.BLASTS - i,
    }
    if i > 1 then
      a.time = a.time + math.random() * 0.05 - 0.025
    end
    self.animation[i] = a
  end
end

function Blast:init(level, pos)
  local backing = Sprite.init(self, level, pos)

  self.time = Blast.TIME - math.random() * 0.05

  self:init_animation()
  self:animate(backing)
end

function Blast:animate(backing)
  local frame = util.get_animation_frame(self.animation, self.time)
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
