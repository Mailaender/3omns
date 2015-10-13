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

local Bot = obj.class("Bot")
package.loaded[...] = Bot

local core     = require("core")
local util     = require("util")
local Entities = require("entities")


function Bot:init(dude, dude_backing, action_time, yield_time)
  self.dude = dude
  self.dude_backing = dude_backing
  self.action_time = action_time
  self.yield_time = yield_time
end

function Bot:co_start()
  -- Wait just a tick to be a little more human at the start.
  self:co_wait(self.action_time + math.random(0, 5) / 10, 0)

  -- The AI system assumes proper tail-call elimination.  I believe I've
  -- structured everything properly, but I haven't tested.
  return self:co_rethink()
end

function Bot:co_yield(elapsed)
  coroutine.yield()
  return elapsed + self.yield_time
end

function Bot:co_wait(till, elapsed)
  while elapsed < till do
    elapsed = self:co_yield(elapsed)
  end
  return elapsed
end

function Bot:get_nearest_dudes(pos)
  local dudes = self.dude:get_nearest(Entities.Dude, pos)
  table.remove(dudes, 1) -- Nix self.
  return dudes
end

function Bot:co_random_walk(interrupt)
  interrupt = interrupt or function() return false end

  local elapsed = 0
  local next_action = 0

  while not interrupt(elapsed) do
    if elapsed >= next_action then
      next_action = next_action + self.action_time

      local dirs = {"u", "d", "l", "r"}
      self.dude:move(dirs[math.random(#dirs)], self.dude_backing)
    end

    elapsed = self:co_yield(elapsed)
  end

  self:co_wait(next_action, elapsed)
end

function Bot:co_move_to(pos, interrupt)
  interrupt = interrupt or function() return false end

  local elapsed = 0
  local next_action = 0
  local path = {}

  util.line(self.dude.pos, pos, function(p)
    if self.dude.entities:walkable(p) then
      path[#path + 1] = p
    end
    return not core.pos_equal(p, pos)
  end)
  table.remove(path, 1) -- Nix current pos.

  while #path > 0 and not interrupt(elapsed) do
    if elapsed >= next_action then
      next_action = next_action + self.action_time

      -- TODO: proper pathfinding that avoids dangers and obstacles.
      local m = {}
      local function add_if_valid(dir)
        if self.dude.entities:walkable(self.dude:move_pos(dir)) then
          m[#m + 1] = dir
        end
      end
      if     path[1].y < self.dude.pos.y then add_if_valid("u")
      elseif path[1].y > self.dude.pos.y then add_if_valid("d") end
      if     path[1].x < self.dude.pos.x then add_if_valid("l")
      elseif path[1].x > self.dude.pos.x then add_if_valid("r") end

      if #m > 1 then
        local dx = math.abs(path[1].x - self.dude.pos.x)
        local dy = math.abs(path[1].y - self.dude.pos.y)
        if dx > dy then
          -- TODO: using underscore.lua would help here.
          local t = {}
          for _, v in ipairs(m) do
            if v ~= "u" and v ~= "d" then t[#t + 1] = v end
          end
          m = t
        elseif dy > dx then
          local t = {}
          for _, v in ipairs(m) do
            if v ~= "l" and v ~= "r" then t[#t + 1] = v end
          end
          m = t
        end
      end

      if #m == 0 then
        if path[1].x ~= self.dude.pos.x then
          add_if_valid("u")
          add_if_valid("d")
        else
          add_if_valid("l")
          add_if_valid("r")
        end
      end

      self.dude:move(m[math.random(#m)], self.dude_backing)

      if core.pos_equal(self.dude.pos, path[1]) then
        table.remove(path, 1)
      end
    end

    elapsed = self:co_yield(elapsed)
  end

  self:co_wait(next_action, elapsed)
end

function Bot:co_fire()
  self.dude:fire(self.dude_backing)

  self:co_wait(self.action_time, 0)
end

function Bot:get_danger(pos)
  pos = pos or self.dude.pos

  if self.dude:is_super() then return nil end

  for _, b in ipairs(self.dude:get_nearest(Entities.Bomn, pos)) do
    if core.blast_dist(pos, b.entity.pos) <= Entities.Bomn.RADIUS then
      return b.entity
    end
  end

  for _, d in ipairs(self:get_nearest_dudes(pos)) do
    if d.dist <= 5
        and (d.entity:is_super() or d.entity.life / self.dude.life > 1.2) then
      return d.entity
    end
  end

  return nil
end

-- TODO: go fishing for supers (or run toward an exposed one) when calm.

function Bot:co_rethink(danger)
  danger = danger or self:get_danger()

  if danger then
    return self:co_run(danger)
  else
    return self:co_hunt()
  end
end

function Bot:co_run(danger)
  local directions = {
    core.Pos( 0, -1),
    core.Pos( 1, -1),
    core.Pos( 1,  0),
    core.Pos( 1,  1),
    core.Pos( 0,  1),
    core.Pos(-1,  1),
    core.Pos(-1,  0),
    core.Pos(-1, -1),
  }

  -- TODO: run toward opponent bomns and disarm if close enough.
  local safe = {}
  for _, d in ipairs(directions) do
    local function safe_callback(p)
      if not self.dude.entities:pos_valid(p) then
        return false
      end
      if not self:get_danger(p) then
        safe[#safe + 1] = {
          dist = core.walk_dist(self.dude.pos, p),
          pos = core.pos_add(p, core.pos_add(d, d)), -- Overshoot a bit.
        }
        return false
      end
      return true
    end

    util.line(self.dude.pos, core.pos_add(self.dude.pos, d), safe_callback)
  end
  table.sort(safe, function(a, b) return a.dist < b.dist end)

  local function interrupt_walk(elapsed)
    return elapsed > 3
  end

  if #safe > 0 then
    self:co_move_to(safe[1].pos, interrupt_walk)
  else
    -- TODO: there should always be a safe place.
    self:co_random_walk(interrupt_walk)
  end

  return self:co_rethink()
end

function Bot:co_hunt()
  local dudes = self:get_nearest_dudes()

  local target
  for _, d in ipairs(dudes) do
    if not self:get_danger(d.entity.pos) then
      target = d.entity
      break
    end
  end

  local function interrupt_walk(elapsed)
    return elapsed > 1 or self:get_danger()
  end

  if target then
    local target_cornered = false
    if self.dude:can_fire() then
      local s = self.dude.entities.level_size
      local corners = {
        core.Pos(1,       1),
        core.Pos(s.width, 1),
        core.Pos(s.width, s.height),
        core.Pos(1,       s.height),
      }

      for _, c in ipairs(corners) do
        local dist = core.blast_dist(self.dude.pos, c)
        if dist <= Entities.Bomn.RADIUS
            and core.blast_dist(target.pos, c) < dist then
          target_cornered = true
          break
        end
      end
    end

    if target_cornered then
      self:co_fire()
    else
      self:co_move_to(target.pos, interrupt_walk)
    end
  else
    -- TODO: a better idle loop.
    self:co_random_walk(interrupt_walk)
  end

  return self:co_rethink()
end
