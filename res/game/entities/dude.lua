local core   = require("core")
local util   = require("util")
local serial = require("serial")
local obj    = require("object")
local Entity = require("entities.entity")
local Bomn   = require("entities.bomn")


local Dude = obj.class("Dude", Entity)

Dude.SUPER_TIME = 10
Dude.BUMP_DAMAGE = 1
Dude.BLAST_DAMAGE = 5

Dude.AI_ACTION_TIME = 0.2

function Dude:init(entities, pos, player)
  local backing = Entity.init(self, entities, nil, pos, 10)

  -- TODO: teams?
  self.player = player
  self.super_time = 0
  self.bomn_id = nil

  self:set_visual(backing)
  self.entities:set_dude(self)
end

function Dude:init_clone(entities, id, pos, life, serialized, start)
  local backing = Entity.init(self, entities, id, pos, life)

  self:sync(serialized, start)

  self:set_visual(backing)
end

function Dude:serialize()
  return serial.serialize_number(self.player)
      .. serial.serialize_number(self.super_time)
      .. serial.serialize_number(self.bomn_id)
end

function Dude:sync(serialized, start)
  self.player,     start = serial.deserialize_number(serialized, start)
  self.super_time, start = serial.deserialize_number(serialized, start)
  self.bomn_id,    start = serial.deserialize_number(serialized, start)
  return start
end

function Dude:set_visual(backing)
  self:set_image(
    (self:is_super() and IMAGES.SUPER_DUDES or IMAGES.DUDES)[self.player],
    backing
  )
  self:set_z_order(1, backing)
end

function Dude:is_super()
  return self.super_time > 0
end

function Dude:can_fire()
  return not self.bomn_id
end

function Dude:superify(backing)
  self:set_image(IMAGES.SUPER_DUDES[self.player], backing)
  self.super_time = Dude.SUPER_TIME

  self:set_dirty(backing)
end

function Dude:unsuperify(backing)
  self:set_image(IMAGES.DUDES[self.player], backing)
  self.super_time = 0

  -- If we ever call unsuperify before the time is actually out, we'll also
  -- need a self:set_dirty(backing) call here.
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

local function ai_yield(ctx, elapsed)
  coroutine.yield()
  return elapsed + ctx.yield_time
end

local function ai_wait(till, ctx, elapsed)
  while elapsed < till do
    elapsed = ai_yield(ctx, elapsed)
  end
  return elapsed
end

function Dude:ai_get_nearest_dudes(ctx, pos)
  local dudes = self:get_nearest(Dude, pos)
  table.remove(dudes, 1) -- Nix self.
  return dudes
end

function Dude:ai_random_walk(ctx, interrupt)
  interrupt = interrupt or function() return false end

  local elapsed = 0
  local next_action = 0

  while not interrupt(ctx, elapsed) do
    if elapsed >= next_action then
      next_action = next_action + Dude.AI_ACTION_TIME

      local dirs = {"u", "d", "l", "r"}
      self:move(dirs[math.random(#dirs)], ctx.backing)
    end

    elapsed = ai_yield(ctx, elapsed)
  end

  ai_wait(next_action, ctx, elapsed)
end

function Dude:ai_move_to(pos, ctx, interrupt)
  interrupt = interrupt or function() return false end

  local elapsed = 0
  local next_action = 0
  local path = {}

  util.line(self.pos, pos, function(p)
    if self.entities:walkable(p) then
      path[#path + 1] = p
    end
    return not core.pos_equal(p, pos)
  end)
  table.remove(path, 1) -- Nix current pos.

  while #path > 0 and not interrupt(ctx, elapsed) do
    if elapsed >= next_action then
      next_action = next_action + Dude.AI_ACTION_TIME

      -- TODO: proper pathfinding that avoids dangers and obstacles.
      local m = {}
      local function add_if_valid(dir)
        if self.entities:walkable(self:move_pos(dir)) then
          m[#m + 1] = dir
        end
      end
      if     path[1].y < self.pos.y then add_if_valid("u")
      elseif path[1].y > self.pos.y then add_if_valid("d") end
      if     path[1].x < self.pos.x then add_if_valid("l")
      elseif path[1].x > self.pos.x then add_if_valid("r") end

      if #m > 1 then
        local dx = math.abs(path[1].x - self.pos.x)
        local dy = math.abs(path[1].y - self.pos.y)
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
        if path[1].x ~= self.pos.x then
          add_if_valid("u")
          add_if_valid("d")
        else
          add_if_valid("l")
          add_if_valid("r")
        end
      end

      self:move(m[math.random(#m)], ctx.backing)

      if core.pos_equal(self.pos, path[1]) then
        table.remove(path, 1)
      end
    end

    elapsed = ai_yield(ctx, elapsed)
  end

  ai_wait(next_action, ctx, elapsed)
end

function Dude:ai_fire(ctx)
  self:fire(ctx.backing)

  ai_wait(Dude.AI_ACTION_TIME, ctx, 0)
end

function Dude:ai_get_danger(ctx, pos)
  pos = pos or self.pos

  if self:is_super() then return nil end

  for _, b in ipairs(self:get_nearest(Bomn, pos)) do
    if core.blast_dist(pos, b.entity.pos) <= Bomn.RADIUS then
      return b.entity
    end
  end

  for _, d in ipairs(self:ai_get_nearest_dudes(ctx, pos)) do
    if d.dist <= 5
        and (d.entity:is_super() or d.entity.life / self.life > 1.2) then
      return d.entity
    end
  end

  return nil
end

function Dude:ai_next_state(ctx, danger)
  danger = danger or self:ai_get_danger(ctx)

  if danger then
    return self:ai_run(ctx, danger)
  else
    return self:ai_hunt(ctx)
  end
end

function Dude:ai_run(ctx, danger)
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
      if not self.entities:pos_valid(p) then
        return false
      end
      if not self:ai_get_danger(ctx, p) then
        safe[#safe + 1] = {
          dist = core.walk_dist(self.pos, p),
          pos = core.pos_add(p, core.pos_add(d, d)), -- Overshoot a bit.
        }
        return false
      end
      return true
    end

    util.line(self.pos, core.pos_add(self.pos, d), safe_callback)
  end
  table.sort(safe, function(a, b) return a.dist < b.dist end)

  local function interrupt_walk(ctx, elapsed)
    return elapsed > 3
  end

  if #safe > 0 then
    self:ai_move_to(safe[1].pos, ctx, interrupt_walk)
  else
    -- TODO: there should always be a safe place.
    self:ai_random_walk(ctx, interrupt_walk)
  end

  return self:ai_next_state(ctx)
end

function Dude:ai_hunt(ctx)
  local dudes = self:ai_get_nearest_dudes(ctx)

  local target
  for _, d in ipairs(dudes) do
    if not self:ai_get_danger(ctx, d.entity.pos) then
      target = d.entity
      break
    end
  end

  local function interrupt_walk(ctx, elapsed)
    return elapsed > 1 or self:ai_get_danger(ctx)
  end

  if target then
    local target_cornered = false
    if self:can_fire() then
      local s = self.entities.level_size
      local corners = {
        core.Pos(1,       1),
        core.Pos(s.width, 1),
        core.Pos(s.width, s.height),
        core.Pos(1,       s.height),
      }

      for _, c in ipairs(corners) do
        local dist = core.blast_dist(self.pos, c)
        if dist <= Bomn.RADIUS and core.blast_dist(target.pos, c) < dist then
          target_cornered = true
          break
        end
      end
    end

    if target_cornered then
      self:ai_fire(ctx)
    else
      self:ai_move_to(target.pos, ctx, interrupt_walk)
    end
  else
    -- TODO: a better idle loop.
    self:ai_random_walk(ctx, interrupt_walk)
  end

  return self:ai_next_state(ctx)
end

-- TODO: go fishing for supers (or run toward an exposed one) when calm.

function Dude:l3_think(backing, yield_time)
  -- The AI system assumes proper tail-call elimination.  I believe I've
  -- structured everything properly, but I haven't tested.

  local ctx = {
    backing    = backing,
    yield_time = yield_time,
  }

  -- Wait just a tick to be a little more human at the start.
  ai_wait(Dude.AI_ACTION_TIME + math.random(0, 5) / 10, ctx, 0)

  return self:ai_hunt(ctx)
end


return Dude
