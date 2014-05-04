local core   = require("core")
local obj    = require("object")
local Entity = require("entities.entity")


local Dude = obj.class(Entity)

Dude.SUPER_TIME = 10
Dude.BUMP_DAMAGE = 1
Dude.BLAST_DAMAGE = 5

function Dude:init(entities, pos, player)
  -- TODO: teams?
  Entity.init(self, entities, pos, 10, IMAGES.DUDES[player], 1)
  self.player = player
  self.super_time = 0
  self.bomn = nil

  entities:set_dude(self)
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

function Dude:l3_think(backing, elapsed)
  while true do
    print("Player " .. self.player .. " thinking...")
    coroutine.yield()
  end
end


return Dude
