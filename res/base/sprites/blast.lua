--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <https://chazomaticus.github.io/3omns/>
  base - base game logic for 3omns
  Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

  3omns is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  3omns is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  3omns.  If not, see <http://www.gnu.org/licenses/>.
]]

local obj = require("object")
local Sprite = require("sprites.sprite")

local Blast = obj.class("Blast", Sprite)
package.loaded[...] = Blast

local core = require("core")
local util = require("util")


Blast.TIME = 1

function Blast:init_animation()
  self.animation = {}

  local frame_duration = Blast.TIME / #core.IMAGES.BLASTS
  for i, v in ipairs(core.IMAGES.BLASTS) do
    local a = {
      time = Blast.TIME - (i - 1) * frame_duration,
      image = v,
      z_order = #core.IMAGES.BLASTS - i,
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
