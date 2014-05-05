local core   = require("core")
local util   = require("util")
local obj    = require("object")
local Entity = require("entities.entity")


local Dude = obj.class(Entity)

Dude.SUPER_TIME = 10
Dude.BUMP_DAMAGE = 1
Dude.BLAST_DAMAGE = 5

Dude.AI_WALK_TIME = 0.2

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
  if not self.bomn then
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

function Dude:ai_random_walk(ctx, interrupt)
  local elapsed = 0
  local next_move = 0

  while true do
    coroutine.yield()
    elapsed = elapsed + ctx.yield_time

    if interrupt and interrupt(ctx, elapsed) then return end

    if elapsed >= next_move then
      next_move = next_move + Dude.AI_WALK_TIME

      local dirs = {"u", "d", "l", "r"}
      self:move(dirs[math.random(#dirs)], ctx.backing)
    end
  end
end

function Dude:ai_move_to(pos, ctx, interrupt)
  local elapsed = 0
  local next_move = 0
  local path = {}

  util.line(self.pos, pos, function(p)
    path[#path + 1] = p
    return not core.pos_equal(p, pos)
  end)
  table.remove(path, 1) -- Nix current pos.

  while #path > 0 do
    coroutine.yield()
    elapsed = elapsed + ctx.yield_time

    if interrupt and interrupt(ctx, elapsed) then return end

    if elapsed >= next_move then
      next_move = next_move + Dude.AI_WALK_TIME

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
  end
end

function Dude:ai_hunt(ctx)
  local dudes = self.entities:get_nearest(self.pos, Dude)
  table.remove(dudes, 1) -- Nix self.

  if #dudes == 0 then
    return self:ai_random_walk(ctx)
  end

  local function interrupt(ctx, elapsed)
    -- TODO: or dude dies or a bomn gets dropped anywhere.
    return elapsed > 1
  end
  self:ai_move_to(dudes[1].entity.pos, ctx, interrupt)
  -- TODO: when opponent is cornered, drop bomn.

  return self:ai_hunt(ctx)
end

-- TODO: go fishing for supers (or run toward an exposed one) when calm.
-- TODO: when super, go on a murder rampage.
-- TODO: when in danger, run away (or toward close opponent bomns).

function Dude:l3_think(backing, yield_time)
  -- The AI system assumes proper tail-call elimination.  I believe I've
  -- structured everything properly, but I haven't tested.

  local ctx = {
    backing    = backing,
    yield_time = yield_time,
  }
  return self:ai_hunt(ctx)
end


return Dude
