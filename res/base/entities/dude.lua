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
local Entity = require("entities.entity")

local Dude = obj.class("Dude", Entity)
package.loaded[...] = Dude

local core = require("core")
local serial = require("serial")
local Entities = require("entities")
local Bot = require("bot")


Dude.SUPER_TIME = 10
Dude.BUMP_DAMAGE = 1
Dude.BLAST_DAMAGE = 5

Dude.AI_ACTION_TIME = 0.2

function Dude:init_base(entities, backing)
  Entity.init_base(self, entities, backing)

  -- TODO: teams?
  self.player = nil
  self.super_time = 0
  self.bomn_id = nil
end

function Dude:init(entities, pos, player)
  Entity.init(self, entities, pos, 10)

  self.player = player
  entities:set_dude(self)

  self:set_visual(backing)
end

function Dude:serialize()
  return serial.serialize_number(self.player)
      .. serial.serialize_number(self.super_time)
      .. serial.serialize_number(self.bomn_id)
end

function Dude:sync(serialized, start, backing)
  self.player, start = serial.deserialize_number(serialized, start)
  self.super_time, start = serial.deserialize_number(serialized, start)
  self.bomn_id, start = serial.deserialize_number(serialized, start)

  -- Don't worry about set_dude()-ing here -- the level already knows what dude
  -- is what, since it was sync'd with the map data.

  self:set_visual(backing)

  return start
end

function Dude:set_visual(backing)
  local image = self:is_super()
      and core.IMAGES.SUPER_DUDES
      or core.IMAGES.DUDES

  self:set_image(image[self.player], backing)
  self:set_z_order(1, backing)
  return self
end

function Dude:is_super()
  return self.super_time > 0
end

function Dude:can_fire()
  return not self.bomn_id
end

function Dude:superify(backing)
  self.super_time = Dude.SUPER_TIME

  self:set_dirty(backing)
  self:set_visual(backing)
end

function Dude:unsuperify(backing)
  self.super_time = 0

  -- If we ever call unsuperify before the time is actually out, we'll also
  -- need a self:set_dirty(backing) call here.
  self:set_visual(backing)
end

function Dude:bumped(other_dude, other_dude_backing)
  if not self:is_super() then
    self:hurt(Dude.BUMP_DAMAGE)
  elseif not other_dude:is_super() then
    other_dude:hurt(Dude.BUMP_DAMAGE, other_dude_backing)
  end
  return false
end

function Dude:blasted(bomn, bomn_backing)
  -- TODO: vary damage by distance from epicenter.
  if not self:is_super() then
    self:hurt(Dude.BLAST_DAMAGE)
  end
end

function Dude:move_pos(action)
  local dir
  if     action == "u" then dir = core.Pos( 0, -1)
  elseif action == "d" then dir = core.Pos( 0,  1)
  elseif action == "l" then dir = core.Pos(-1,  0)
  else                      dir = core.Pos( 1,  0) end
  return core.pos_add(self.pos, dir)
end

function Dude:move(action, backing)
  local new_pos = self:move_pos(action)
  if not self.entities:walkable(new_pos) then return end

  -- This is kind of weird, but because bumping can cause an arbitrary shift in
  -- entities (e.g. crates being destroyed spawning supers), we need to keep
  -- looping until it returns false (meaning we're blocked), or either there's
  -- no entity there or we see the same entity twice in a row (meaning there
  -- was no shift of entities, and we aren't blocked).
  local last = nil
  local other = nil
  repeat
    last = other
    other = self.entities:get_entity(new_pos)
    if not other or last == other then
      self:set_pos(new_pos)
      break
    end
  until not other:bumped(self, backing)
end

function Dude:fire(backing)
  -- TODO: when super, fire should trigger a blast immediately, allowable every
  -- one second.
  if self:can_fire() then
    local bomn = self.entities:Bomn(self.pos, self.id)
    self.bomn_id = bomn.id

    self:set_dirty(backing)
  end
end

function Dude:l3_update(backing, elapsed)
  if self.bomn_id and not self.entities:exists(self.bomn_id) then
    self.bomn_id = nil
  end

  if self:is_super() then
    self.super_time = self.super_time - elapsed
    if self.super_time <= 0 then
      self:unsuperify(backing)
    end
  end
end

function Dude:l3_action(backing, action)
  if action == "f" then
    self:fire(backing)
  else
    self:move(action, backing)
  end
end

function Dude:l3_co_think(backing, elapsed)
  return Bot(self, backing, Dude.AI_ACTION_TIME):co_start(elapsed)
end
