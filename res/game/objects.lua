local function new(class, ...)
  local self = setmetatable({}, class)
  self:init(...)
  return self
end

local function is_a(object, class)
  -- FIXME: handle inheritance (this doesn't work when checking superclasses).
  return getmetatable(object) == class
end

local function class(parent)
  local c = {}
  c.__index = c

  setmetatable(c, {
    __index = parent,
    __call = new,
  })

  return c
end

local function valid_pos(pos, level_size)
  return pos.x >= 1 and pos.x <= level_size.width
      and pos.y >= 1 and pos.y <= level_size.height
end


local Sprite = class()
local Blast  = class(Sprite)

Entities = class()

local Entity = class()
local Super  = class(Entity)
local Crate  = class(Entity)
local Bomn   = class(Entity)
local Dude   = class(Entity)

local public = {
  Super = Super,
  Crate = Crate,
  Bomn  = Bomn,
  Dude  = Dude,
}

for name, constructor in pairs(public) do
  Entities[name] = function(self, ...)
    return constructor(self, ...)
  end
end


function Sprite:is_a(class)
  return is_a(self, class)
end

function Sprite:init(level, pos, image)
  local backing = level:new_sprite(self)
  self:set_pos(pos, backing)
  self:set_image(image, backing)
end

function Sprite:set_pos(pos, backing)
  self.pos = pos
  backing:set_pos(pos)

  return self
end

function Sprite:set_image(image, backing)
  self.image = image
  backing:set_image(image)

  return self
end

function Sprite:destroy(backing)
  backing:destroy()
end


Blast.TIME = 1

function Blast:init(level, pos)
  Sprite.init(self, level, pos, IMAGES.BLASTS[1])
  self.time = Blast.TIME
end

function Blast:l3_update(backing, elapsed)
  self.time = self.time - elapsed
  if self.time <= 0 then
    self:destroy(backing)
    return
  end

  -- TODO: animate.
end


function Entities:init(level)
  self.level = level
  self.level_size = level:get_size()
  self.index = {}
  self.dudes = {}
end

-- The C entities are referred to as the "backing" for Lua entities.  Deal.
-- Backings should never be stored except as a temporary local.  If you store
-- the Lua entity, use Entity:exists() to double check the backing hasn't been
-- destroyed in C while your back was turned.
function Entities:new_backing(entity)
  local backing = self.level:new_entity(entity)
  return backing:get_id(), backing
end

function Entities:get_backing(id)
  return self.level:get_entity(id)
end

function Entities:set_dude(dude)
  self.level:set_dude(dude.player, dude.id)
  self.dudes[dude.id] = dude
end

local function entities_index_key(pos)
  return string.format("%x,%x", pos.x, pos.y)
end

function Entities:get_entity(pos)
  for _, d in pairs(self.dudes) do
    if pos_equal(pos, d.pos) then return d end
  end

  return self.index[entities_index_key(pos)]
end

function Entities:move(entity, old_pos)
  if entity:is_a(Dude) then return end

  if old_pos then
    self.index[entities_index_key(old_pos)] = nil
  end

  local key = entities_index_key(entity.pos)
  assert(not self.index[key], "Two entities can't occupy the same space")
  self.index[key] = entity
end

function Entities:remove(entity)
  if entity:is_a(Dude) then
    self.dudes[entity.id] = nil
  else
    self.index[entities_index_key(entity.pos)] = nil
  end
end

function Entities:walkable(pos)
  return valid_pos(pos, self.level_size)
      and self.level:get_tile(pos) ~= TILES.WALL
end


function Entity:is_a(class)
  return is_a(self, class)
end

function Entity:init(entities, pos, life, image)
  self.entities = entities
  local backing
  self.id, backing = entities:new_backing(self)
  self:set_pos(pos, backing)
  self:set_life(life, backing)
  self:set_image(image, backing)
end

function Entity:get_backing()
  return self.entities:get_backing(self.id)
end

function Entity:exists()
  return self:get_backing() ~= nil
end

function Entity:set_pos(pos, backing)
  backing = backing or self:get_backing()

  if self.pos == nil or not pos_equal(pos, self.pos) then
    local old = self.pos

    self.pos = pos
    backing:set_pos(pos)

    self.entities:move(self, old)
  end

  return self
end

function Entity:set_life(life, backing)
  backing = backing or self:get_backing()

  if life ~= self.life then
    self.life = life
    backing:set_life(life)

    if life == 0 then
      self.entities:remove(self)
      if self.killed then
        self:killed()
      end
    end
  end

  return self
end

function Entity:set_image(image, backing)
  backing = backing or self:get_backing()

  if image ~= self.image then
    self.image = image
    backing:set_image(image)
  end

  return self
end

function Entity:kill(backing)
  self:set_life(0, backing)
end


Super.TIME = 10

function Super:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.SUPER)
end


function Crate:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.CRATE)
  self.killed = nil
end

function Crate:carry(f)
  self.killed = f
end

function Crate:carries()
  return self.killed
end


Bomn.TIME = 3

function Bomn:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.BOMNS[Bomn.TIME])
  self.time = Bomn.TIME
end

function Bomn:explode(backing)
  self:kill(backing)
  -- TODO: damage, spawn sprites.
end

function Bomn:l3_update(backing, elapsed)
  self.time = self.time - elapsed
  if self.time <= 0 then
    self:explode(backing)
    return
  end

  -- TODO: animate.
end


function Dude:init(entities, pos, player)
  Entity.init(self, entities, pos, 10, IMAGES.DUDES[player])
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
  self.super_time = Super.TIME
end

function Dude:unsuperify(backing)
  self:set_image(IMAGES.DUDES[self.player], backing)
  self.super_time = 0
end

function Dude:bump(other, backing)
  if other:is_a(Crate) then
    other:kill()
    return self:is_super()
  elseif other:is_a(Super) then
    other:kill()
    self:superify(backing)
    return true
  end
  -- TODO
end

function Dude:move(direction, backing)
  local dir
  if     direction == "u" then dir = Pos( 0, -1)
  elseif direction == "d" then dir = Pos( 0,  1)
  elseif direction == "l" then dir = Pos(-1,  0)
  else                         dir = Pos( 1,  0) end

  local new_pos = Pos(self.pos.x + dir.x, self.pos.y + dir.y)
  if not self.entities:walkable(new_pos) then return end

  repeat
    local other = self.entities:get_entity(new_pos)
    if not other then
      self:set_pos(new_pos)
      break
    end
  until not self:bump(other, backing)
end

function Dude:fire()
  if not self.bomn then
    self.bomn = self.entities:Bomn(self.pos)
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
