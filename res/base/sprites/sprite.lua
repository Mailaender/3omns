--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <http://chazomaticus.github.io/3omns/>
  base - base game logic for 3omns
  Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

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

local Sprite = obj.class("Sprite")
package.loaded[...] = Sprite

local core = require("core")


function Sprite:init(level, pos)
  local backing = level:new_sprite(self)

  self:set_pos(pos, backing)

  return backing
end

Sprite.get_type = obj.get_type

function Sprite:set_pos(pos, backing)
  if not self.pos or not core.pos_equal(pos, self.pos) then
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
