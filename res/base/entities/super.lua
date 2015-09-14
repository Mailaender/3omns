--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <http://chazomaticus.github.io/3omns/>
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
