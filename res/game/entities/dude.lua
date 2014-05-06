local core   = require("core")
local util   = require("util")
local obj    = require("object")
local Entity = require("entities.entity")
local Bomn   = require("entities.bomn")


local Dude = obj.class(Entity)

Dude.SUPER_TIME = 10
Dude.BUMP_DAMAGE = 1
Dude.BLAST_DAMAGE = 5

Dude.AI_ACTION_TIME = 0.2

function Dude:init(entities, pos, player)
  -- TODO: teams?
  self.player = player
  self.super_time = 0
  self.bomn = nil

  Entity.init(self, entities, pos, 10, IMAGES.DUDES[player], 1)
end

function Dude:is_super()
  return self.super_time > 0
end

function Dude:can_fire()
  return self.bomn == nil
end

function Dude:superify(backing)
  self:set_image(IMAGES.SUPER_DUDES[self.player], backing)
  self.super_time = Dude.SUPER_TIME
end

function Dude:unsuperify(backing)
  self:set_image(IMAGES.DUDES[self.player], backing)
  self.super_time = 0
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

function Dude:move(direction, backing)
  local dir
  if     direction == "u" then dir = core.Pos( 0, -1)
  elseif direction == "d" then dir = core.Pos( 0,  1)
  elseif direction == "l" then dir = core.Pos(-1,  0)
  else                         dir = core.Pos( 1,  0) end

  local new_pos = core.Pos(self.pos.x + dir.x, self.pos.y + dir.y)
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

function Dude:fire()
  -- TODO: when super, fire should trigger a blast immediately, allowable every
  -- one second.
  if self:can_fire() then
    self.bomn = self.entities:Bomn(self.pos, self)
  end
end

function Dude:l3_update(backing, elapsed)
  if self.bomn and not self.bomn:exists() then
    self.bomn = nil
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
    self:fire()
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
    path[#path + 1] = p
    return not core.pos_equal(p, pos)
  end)
  table.remove(path, 1) -- Nix current pos.

  while #path > 0 and not interrupt(ctx, elapsed) do
    if elapsed >= next_action then
      next_action = next_action + Dude.AI_ACTION_TIME

      -- TODO: avoid obstacles.
      local m = {}
      if     path[1].y < self.pos.y then m[#m + 1] = "u"
      elseif path[1].y > self.pos.y then m[#m + 1] = "d" end
      if     path[1].x < self.pos.x then m[#m + 1] = "l"
      elseif path[1].x > self.pos.x then m[#m + 1] = "r" end

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
  self:fire()

  ai_wait(Dude.AI_ACTION_TIME, ctx, 0)
end

function Dude:ai_hunt(ctx)
  local dudes = self.entities:get_nearest(self.pos, Dude)
  table.remove(dudes, 1) -- Nix self.

  local target
  for _, d in ipairs(dudes) do
    if not d.entity:is_super() then
      target = d.entity
      break
    end
  end

  local function interrupt_walk(ctx, elapsed)
    -- TODO: or we're in danger.
    return elapsed > 1
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

  return self:ai_hunt(ctx)
end

-- TODO: go fishing for supers (or run toward an exposed one) when calm.
-- TODO: when in danger, run away (or toward close opponent bomns).

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
