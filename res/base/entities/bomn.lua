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

local obj = require("object")
local Entity = require("entities.entity")

local Bomn = obj.class("Bomn", Entity)
package.loaded[...] = Bomn

local core = require("core")
local util = require("util")
local serial = require("serial")
local Blast = require("sprites.blast")


Bomn.TIME = 3
Bomn.RADIUS = 8

Bomn.ANIMATION = nil

local function init_animation()
  if Bomn.ANIMATION then return end

  Bomn.ANIMATION = {
    {time = 5.0,  image = core.IMAGES.BOMNS[5],                  z_order = 2},
    {time = 4.75, image = nil,                                   z_order = 2},
    {time = 4.5,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
    {time = 4.25, image = nil,                                   z_order = 2},
    {time = 4.0,  image = core.IMAGES.BOMNS[4],                  z_order = 2},
    {time = 3.75, image = nil,                                   z_order = 2},
    {time = 3.5,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
    {time = 3.25, image = nil,                                   z_order = 2},
    {time = 3.0,  image = core.IMAGES.BOMNS[3],                  z_order = 2},
    {time = 2.75, image = nil,                                   z_order = 2},
    {time = 2.5,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
    {time = 2.25, image = nil,                                   z_order = 2},
    {time = 2.0,  image = core.IMAGES.BOMNS[2],                  z_order = 2},
    {time = 1.75, image = nil,                                   z_order = 2},
    {time = 1.5,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
    {time = 1.25, image = nil,                                   z_order = 2},
    {time = 1.0,  image = core.IMAGES.BOMNS[1],                  z_order = 2},
    {time = 0.75, image = nil,                                   z_order = 2},
    {time = 0.5,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
    {time = 0.4,  image = nil,                                   z_order = 2},
    {time = 0.3,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
    {time = 0.2,  image = nil,                                   z_order = 2},
    {time = 0.1,  image = core.IMAGES.BOMNS[#core.IMAGES.BOMNS], z_order = 2},
  }
end

function Bomn:init_base(entities, backing)
  init_animation()

  Entity.init_base(self, entities, backing)

  self.solid = false
  self.time = Bomn.TIME
  self.dude_id = nil

  self:animate(backing)
end

function Bomn:init(entities, pos, dude_id)
  Entity.init(self, entities, pos, 1)

  self.dude_id = dude_id
end

function Bomn:serialize()
  return serial.serialize_number(self.time)
      .. serial.serialize_number(self.dude_id)
end

function Bomn:sync(serialized, start, backing)
  self.time, start = serial.deserialize_number(serialized, start)
  self.dude_id, start = serial.deserialize_number(serialized, start)
  return start
end

function Bomn:animate(backing)
  local frame = util.get_animation_frame(Bomn.ANIMATION, self.time)
  self:set_image(frame.image, backing)
  self:set_z_order(frame.z_order, backing)
end

-- TODO: if the server deletes this entity before it explodes, we get no
-- explosion locally (though the effect is exactly the same, since it does
-- happen on the server).
function Bomn:explode(backing)
  self:kill(backing)

  local blasts = {}
  local to_blast = {}
  util.circle_contig(self.pos, Bomn.RADIUS, function(edge_pos)
    util.line(self.pos, edge_pos, function(pos)
      if not self.entities:walkable(pos) then
        return false
      end

      local key = core.pos_key(pos)
      if not blasts[key] then
        blasts[key] = pos
      end

      local other = self.entities:get_entity(pos)
      if other and other.solid then
        if not to_blast[key] then
          to_blast[key] = other
        end

        return core.pos_equal(pos, self.pos)
      end

      return not core.pos_equal(pos, edge_pos)
    end)
  end)

  for _, e in pairs(to_blast) do
    e:blasted(self, backing)
  end

  for _, p in pairs(blasts) do
    Blast(self.entities.level, p)
  end
end

function Bomn:bumped(dude, dude_backing)
  if self.dude_id ~= dude.id then
    self:kill()
  end
  return true
end

function Bomn:l3_update(backing, elapsed)
  self.time = self.time - elapsed
  if self.time <= 0 then
    self:explode(backing)
    return
  end

  self:animate(backing)
end
