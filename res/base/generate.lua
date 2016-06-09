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

local gen = {}
package.loaded[...] = gen

local obj = require("object")
local core = require("core")
local util = require("util")
local Entities = require("entities")


local MAP_SIZE = core.Size(30, 30)
local MAX_ENTITIES = MAP_SIZE.width * MAP_SIZE.height + 4

local Generator = obj.class("Generator")

function gen.generate()
  local level = l3.level.new(MAP_SIZE, MAX_ENTITIES)
  Generator(level, Entities(level)):generate()
  return level
end

function Generator:init(level, entities)
  self.level = level
  self.entities = entities
  self.spawns = {}
  self.dudes = {}
  self.crates = {}
end

function Generator:generate()
  self:set_spawns()
  self:add_walls()
  self:fill_space()
  self:spawn_dudes()
  self:spawn_crates()
  self:fill_crates()
end

function Generator:set_spawns()
  local quads = {
    core.Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2),
    core.Pos(0, 0),
    core.Pos(MAP_SIZE.width / 2, 0),
    core.Pos(0, MAP_SIZE.height / 2),
  }

  for i, q in ipairs(quads) do
    self.spawns[i] = core.Pos(
      math.random(q.x + 2, q.x + MAP_SIZE.width  / 2 - 2),
      math.random(q.y + 2, q.y + MAP_SIZE.height / 2 - 2)
    )
  end
end

function Generator:spawn_at(pos)
  for _, s in ipairs(self.spawns) do
    if core.pos_equal(pos, s) then return true end
  end
  return false
end

function Generator:add_walls()
  local fourth = core.Size(
    math.floor(MAP_SIZE.width  / 4),
    math.floor(MAP_SIZE.height / 4)
  )
  local pos = {
    core.Pos(fourth.width, fourth.height),
    core.Pos(fourth.width, MAP_SIZE.height - fourth.height),
    core.Pos(MAP_SIZE.width - fourth.width, fourth.height),
    core.Pos(MAP_SIZE.width - fourth.width, MAP_SIZE.height - fourth.height),
  }

  local function wall_grid(center)
    local walls = {}
    for x = center.x - 3, center.x + 3, 3 do
      for y = center.y - 3, center.y + 3, 3 do
        if not self:spawn_at(core.Pos(x, y)) then
          walls[#walls + 1] = core.Pos(x, y)
        end
      end
    end

    while #walls > 5 do
      table.remove(walls, math.random(#walls))
    end

    for _, w in ipairs(walls) do
      self.level:set_tile(w, core.TILES.WALL)
    end
  end

  for _, p in ipairs(pos) do
    wall_grid(p)
  end
  wall_grid(core.Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2))
end

function Generator:fill_space()
  for x = 1, MAP_SIZE.width do
    for y = 1, MAP_SIZE.height do
      if self.level:get_tile(core.Pos(x, y)) == 0 then
        self.level:set_tile(core.Pos(x, y), core.TILES.BLANK)
      end
    end
  end
end

function Generator:spawn_dudes()
  for i, s in ipairs(self.spawns) do
    self.dudes[i] = self.entities:Dude(s, i)
  end
end

function Generator:is_empty(pos)
  return self.level:get_tile(pos) ~= core.TILES.WALL
      and not self.entities:get_entity(pos)
end

function Generator:spawn_crates()
  local function bisect(a, b)
    -- Rounded up or down at random.
    return core.Pos(
      math.floor((a.x + b.x) / 2 + 0.5 * math.random(0, 1)),
      math.floor((a.y + b.y) / 2 + 0.5 * math.random(0, 1))
    )
  end

  local span_dir
  local function crates(pos)
    if not self.entities:pos_valid(pos) then return false end

    local start = core.Pos(
      pos.x + span_dir.x * math.random(-2, -1),
      pos.y + span_dir.y * math.random(-2, -1)
    )

    for i = 0, 3 do
      local p = core.Pos(start.x + span_dir.x * i, start.y + span_dir.y * i)
      if self.entities:pos_valid(p) and self:is_empty(p) then
        self.crates[#self.crates + 1] = self.entities:Crate(p)
      end
    end

    return true
  end

  local center = core.Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2)

  span_dir = core.Pos(0, 1)
  util.line(center, bisect(self.spawns[1], self.spawns[3]), crates)

  span_dir = core.Pos(0, 1)
  util.line(center, bisect(self.spawns[2], self.spawns[4]), crates)

  span_dir = core.Pos(1, 0)
  util.line(center, bisect(self.spawns[2], self.spawns[3]), crates)

  span_dir = core.Pos(1, 0)
  util.line(center, bisect(self.spawns[1], self.spawns[4]), crates)
end

function Generator:fill_crates()
  for i = 1, #self.spawns do
    local c
    repeat
      c = self.crates[math.random(#self.crates)]
    until not c:holds()

    c:hold(self.entities.Super)
  end
end
