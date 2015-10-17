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

local gen = {}
package.loaded[...] = gen

local core = require("core")
local util = require("util")
local Entities = require("entities")


local MAP_SIZE = core.Size(30, 30)
local MAX_ENTITIES = MAP_SIZE.width * MAP_SIZE.height + 4

local function generate_spawns(ctx)
  local quads = {
    core.Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2),
    core.Pos(0, 0),
    core.Pos(MAP_SIZE.width / 2, 0),
    core.Pos(0, MAP_SIZE.height / 2),
  }

  for i, q in ipairs(quads) do
    ctx.spawns[i] = core.Pos(
      math.random(q.x + 2, q.x + MAP_SIZE.width  / 2 - 2),
      math.random(q.y + 2, q.y + MAP_SIZE.height / 2 - 2)
    )
  end
end

local function spawn_at(ctx, pos)
  for _, s in ipairs(ctx.spawns) do
    if core.pos_equal(pos, s) then return true end
  end
  return false
end

local function wall_grid(ctx, center)
  local walls = {}
  for x = center.x - 3, center.x + 3, 3 do
    for y = center.y - 3, center.y + 3, 3 do
      if not spawn_at(ctx, core.Pos(x, y)) then
        walls[#walls + 1] = core.Pos(x, y)
      end
    end
  end

  while #walls > 5 do
    table.remove(walls, math.random(#walls))
  end

  for _, w in ipairs(walls) do
    ctx.level:set_tile(w, TILES.WALL)
  end
end

local function generate_walls(ctx)
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

  for _, p in ipairs(pos) do
    wall_grid(ctx, p)
  end
  wall_grid(ctx, core.Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2))
end

local function fill_space(ctx)
  for x = 1, MAP_SIZE.width do
    for y = 1, MAP_SIZE.height do
      if ctx.level:get_tile(core.Pos(x, y)) == 0 then
        ctx.level:set_tile(core.Pos(x, y), TILES.BLANK)
      end
    end
  end
end

local function spawn_dudes(ctx)
  for i, s in ipairs(ctx.spawns) do
    ctx.dudes[i] = ctx.entities:Dude(s, i)
  end
end

local function valid(pos)
  return pos.x >= 1 and pos.x <= MAP_SIZE.width
      and pos.y >= 1 and pos.y <= MAP_SIZE.height
end

local function empty(ctx, pos)
  return ctx.level:get_tile(pos) ~= TILES.WALL
      and not ctx.entities:get_entity(pos)
end

local function bisect(a, b)
  -- Rounded up or down at random.
  return core.Pos(
    math.floor((a.x + b.x) / 2 + 0.5 * math.random(0, 1)),
    math.floor((a.y + b.y) / 2 + 0.5 * math.random(0, 1))
  )
end

local function spawn_crates(ctx)
  local span_dir
  local function crates(pos)
    if not valid(pos) then return false end

    local start = core.Pos(
      pos.x + span_dir.x * math.random(-2, -1),
      pos.y + span_dir.y * math.random(-2, -1)
    )

    for i = 0, 3 do
      local p = core.Pos(start.x + span_dir.x * i, start.y + span_dir.y * i)
      if valid(p) and empty(ctx, p) then
        ctx.crates[#ctx.crates + 1] = ctx.entities:Crate(p)
      end
    end

    return true
  end

  local center = core.Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2)
  local bisection

  bisection = bisect(ctx.spawns[1], ctx.spawns[3])
  span_dir = core.Pos(0, 1)
  util.line(center, bisection, crates)

  bisection = bisect(ctx.spawns[2], ctx.spawns[4])
  span_dir = core.Pos(0, 1)
  util.line(center, bisection, crates)

  bisection = bisect(ctx.spawns[2], ctx.spawns[3])
  span_dir = core.Pos(1, 0)
  util.line(center, bisection, crates)

  bisection = bisect(ctx.spawns[1], ctx.spawns[4])
  span_dir = core.Pos(1, 0)
  util.line(center, bisection, crates)
end

local function fill_crates(ctx)
  for i = 1, #ctx.spawns do
    local c
    repeat
      c = ctx.crates[math.random(#ctx.crates)]
    until not c:holds()

    c:hold(ctx.entities.Super)
  end
end

function gen.generate()
  local level = l3.level.new(MAP_SIZE, MAX_ENTITIES)
  local ctx = {
    level = level,
    entities = Entities(level),
    spawns = {},
    dudes = {},
    crates = {},
  }

  generate_spawns(ctx)
  generate_walls(ctx)
  fill_space(ctx)
  spawn_dudes(ctx)
  spawn_crates(ctx)
  fill_crates(ctx)

  return level
end
