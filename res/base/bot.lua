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

local Bot = obj.class("Bot")
package.loaded[...] = Bot

local core = require("core")
local util = require("util")
local Entities = require("entities")


function Bot:init(dude, dude_backing, action_time)
  self.dude = dude
  self.dude_backing = dude_backing
  self.action_time = action_time
end

function Bot:co_start(elapsed)
  -- Wait just a tick (beyond what's already elapsed, which we ignore) to be a
  -- little more human at the start.
  self:co_wait(self.action_time + math.random(0, 5) / 10)

  while true do
    self:co_rethink()
  end
end

function Bot:co_rethink(danger)
  danger = danger or self:get_danger()

  if danger then
    return self:co_run_away(danger)
  end

  -- TODO: go fishing for supers (or run toward an exposed one) when calm.
  return self:co_hunt()
end

function Bot:do_until(act, done)
  local elapsed = 0
  while not done(elapsed) do
    elapsed = elapsed + act()
  end
  return elapsed
end

function Bot:co_wait(duration)
  return self:do_until(
    -- l3_think_agent pushes the elapsed time yielded so coroutine.yield() will
    -- return it.
    coroutine.yield,
    function(elapsed) return elapsed >= duration end
  )
end

function Bot:co_act(action)
  self.dude:l3_action(self.dude_backing, action)

  return self:co_wait(self.action_time)
end

function Bot:co_fire()
  return self:co_act("f")
end

Bot.co_move = Bot.co_act -- Assume action is u/d/l/r, but it's w/e.

function Bot:get_nearest_dudes(pos)
  local dudes = self.dude:get_nearest(Entities.Dude, pos)
  return util.filter_array(dudes, function (d)
    return d.entity ~= self.dude
  end)
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

function Bot:co_random_walk(interrupt)
  local actions = {"u", "d", "l", "r"}
  return self:do_until(function()
    return self:co_move(actions[math.random(#actions)])
  end, interrupt)
end

function Bot:co_move_to(pos, interrupt)
  local path = {}
  util.line(self.dude.pos, pos, function(p)
    if self.dude.entities:walkable(p) then
      path[#path + 1] = p
    end
    return not core.pos_equal(p, pos)
  end)
  table.remove(path, 1) -- Nix current pos.

  -- TODO: proper pathfinding that avoids dangers and obstacles.
  local function get_valid_actions_for_next_step()
    local valid_actions = {}
    local function add_action_if_valid(action)
      if self.dude.entities:walkable(self.dude:move_pos(action)) then
        valid_actions[#valid_actions + 1] = action
      end
    end
    if     path[1].y < self.dude.pos.y then add_action_if_valid("u")
    elseif path[1].y > self.dude.pos.y then add_action_if_valid("d") end
    if     path[1].x < self.dude.pos.x then add_action_if_valid("l")
    elseif path[1].x > self.dude.pos.x then add_action_if_valid("r") end

    -- In case of a non-walkable obstacle, there will be a discontinuity in
    -- path.  This makes sure we go around it the most efficient way.
    if #valid_actions > 1 then
      local dx = math.abs(path[1].x - self.dude.pos.x)
      local dy = math.abs(path[1].y - self.dude.pos.y)
      if dx > dy then
        valid_actions = util.filter_array(valid_actions, function (v)
          return v ~= "u" and v ~= "d"
        end)
      elseif dy > dx then
        valid_actions = util.filter_array(valid_actions, function (v)
          return v ~= "l" and v ~= "r"
        end)
      end
    end

    -- In case of an obstacle directly in our path, move around it any way
    -- possible.
    if #valid_actions == 0 then
      if path[1].x ~= self.dude.pos.x then
        add_action_if_valid("u")
        add_action_if_valid("d")
      else
        add_action_if_valid("l")
        add_action_if_valid("r")
      end
    end

    return valid_actions
  end

  local function co_step()
    -- TODO: need a contingency plan for no valid_actions.  This could happen
    -- if there were e.g. one wall to our right and one below us, and the next
    -- path point was down and to the right.  It won't happen with the current
    -- way levels are generated, but it's a possibility in general.  See also:
    -- need proper pathfinding.
    local valid_actions = get_valid_actions_for_next_step()
    local elapsed = self:co_move(valid_actions[math.random(#valid_actions)])

    if core.pos_equal(self.dude.pos, path[1]) then
      table.remove(path, 1)
    end

    return elapsed
  end

  return self:do_until(
    co_step,
    function(elapsed) return #path == 0 or interrupt(elapsed) end
  )
end

function Bot:co_run_away(danger)
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
  -- TODO: omit paths (unless it's the only one) that go through a dude.

  local function interrupt(elapsed)
    return elapsed > 3
  end

  if #safe == 0 then
    -- TODO: there should always be a safe place.
    return self:co_random_walk(interrupt)
  end

  return self:co_move_to(safe[1].pos, interrupt)
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

  local function interrupt(elapsed)
    return elapsed > 1 or self:get_danger()
  end

  if not target then
    return self:co_random_walk(interrupt)
  end

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
    return self:co_fire()
  end

  return self:co_move_to(target.pos, interrupt)
end
