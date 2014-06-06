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

local obj    = require("object")
local Entity = require("entities.entity")

local Crate = obj.class("Crate", Entity)
package.loaded[...] = Crate

local serial = require("serial")


function Crate:init_base(entities, backing)
  Entity.init_base(self, entities, backing)

  -- "Held" in the sense that when the crate is destroyed, an instance of the
  -- given entity type is called with this crate's pos in the constructor and
  -- nothing after it.
  self.held_type = nil

  self:set_image(IMAGES.CRATE, backing)
  self:set_z_order(0, backing)
end

function Crate:init(entities, pos, held_type)
  Entity.init(self, entities, pos, 1)

  self.held_type = held_type
end

function Crate:serialize()
  return serial.serialize_type(self.held_type)
end

function Crate:sync(serialized, start, backing)
  self.held_type, start = serial.deserialize_type(serialized, start)
  return start
end

function Crate:hold(type)
  self.held_type = type

  self:set_dirty()
end

function Crate:holds()
  return self.held_type
end

function Crate:killed()
  if self.held_type then
    self.held_type(self.entities, self.pos)
  end
end

function Crate:bumped(dude, dude_backing)
  self:kill()
  return dude:is_super()
end
