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

-- Returns a value suitable for using a Pos as a key in a table.
local function pos_key(pos)
  return string.format("%x,%x", pos.x, pos.y)
end

-- Time counts down for these objects.  It's more of a "lifetime remaining".
local function animate(self, backing, time, old_time, animation)
  for _, a in ipairs(animation) do
    if a.time < time then break end

    if old_time > a.time and time <= a.time then
      self:set_image(a.image, backing)
      self:set_z_order(a.z_order, backing)
      break
    end
  end
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
  Entities[name] = constructor
end


function Sprite:is_a(class)
  return is_a(self, class)
end

function Sprite:init(level, pos, image, z_order)
  local backing = level:new_sprite(self)
  self:set_pos(pos, backing)
  self:set_image(image, backing)
  self:set_z_order(z_order, backing)
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

function Sprite:set_z_order(z_order, backing)
  self.z_order = z_order
  backing:set_z_order(z_order)

  return self
end

function Sprite:destroy(backing)
  backing:destroy()
end


Blast.TIME = 1

Blast.ANIMATION = {}
local blast_frame_duration = Blast.TIME / #IMAGES.BLASTS
for i, v in ipairs(IMAGES.BLASTS) do
  Blast.ANIMATION[i] = {
    time = Blast.TIME - (i - 1) * blast_frame_duration,
    image = v,
    z_order = #IMAGES.BLASTS - i,
  }
end

function Blast:init(level, pos)
  Sprite.init(self, level, pos, IMAGES.BLASTS[1], #IMAGES.BLASTS - 1)
  self.time = Blast.TIME
end

function Blast:animate(backing, old_time)
  animate(self, backing, self.time, old_time, Blast.ANIMATION)
end

function Blast:l3_update(backing, elapsed)
  local old_time = self.time

  self.time = self.time - elapsed
  if self.time <= 0 then
    self:destroy(backing)
    return
  end

  self:animate(backing, old_time)
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

function Entities:get_tile(pos)
  return self.level:get_tile(pos)
end

function Entities:valid_pos(pos)
  return pos.x >= 1 and pos.x <= self.level_size.width
      and pos.y >= 1 and pos.y <= self.level_size.height
end

function Entities:walkable(pos)
  return self:valid_pos(pos) and self:get_tile(pos) ~= TILES.WALL
end

function Entities:get_entity(pos)
  for _, d in pairs(self.dudes) do
    if pos_equal(pos, d.pos) then return d end
  end

  return self.index[pos_key(pos)]
end

function Entities:move(entity, old_pos)
  if entity:is_a(Dude) then return end

  if old_pos then
    self.index[pos_key(old_pos)] = nil
  end

  local key = pos_key(entity.pos)
  assert(not self.index[key], "Two entities can't occupy the same space")
  self.index[key] = entity
end

function Entities:remove(entity)
  if entity:is_a(Dude) then
    self.dudes[entity.id] = nil
  else
    self.index[pos_key(entity.pos)] = nil
  end
end


function Entity:is_a(class)
  return is_a(self, class)
end

function Entity:init(entities, pos, life, image, z_order)
  self.entities = entities
  local backing
  self.id, backing = entities:new_backing(self)
  self:set_pos(pos, backing)
  self:set_life(life, backing)
  self:set_image(image, backing)
  self:set_z_order(z_order, backing)
  self.solid = true -- Whether blasts are stopped by the entity.
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

function Entity:set_z_order(z_order, backing)
  backing = backing or self:get_backing()

  if z_order ~= self.z_order then
    self.z_order = z_order
    backing:set_z_order(z_order)
  end

  return self
end

function Entity:kill(backing)
  self:set_life(0, backing)
end

function Entity:hurt(damage, backing)
  -- TODO: keep track of who's doing the hurting.
  self:set_life(math.max(0, self.life - damage), backing)
end

function Entity:bumped(dude, dude_backing)
  return false
end

function Entity:blasted(bomn, bomn_backing)
  if self.solid then
    self:kill()
  end
end


Super.TIME = 10

function Super:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.SUPER, 0)
  self.solid = false
end

function Super:bumped(dude, dude_backing)
  self:kill()
  dude:superify(dude_backing)
  return true
end


function Crate:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.CRATE, 0)
  self.killed = nil
end

-- "Hold" in the sense that when the crate is destroyed, the function gets
-- called, presumably to spawn something else.
function Crate:hold(f)
  self.killed = f
end

function Crate:holds()
  return self.killed
end

function Crate:bumped(dude, dude_backing)
  self:kill()
  return dude:is_super()
end


Bomn.TIME = 3
Bomn.RADIUS = 8

Bomn.ANIMATION = {
  {time = 5.0,  image = IMAGES.BOMNS[5],             z_order = 2},
  {time = 4.75, image = nil,                         z_order = 2},
  {time = 4.5,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
  {time = 4.25, image = nil,                         z_order = 2},
  {time = 4.0,  image = IMAGES.BOMNS[4],             z_order = 2},
  {time = 3.75, image = nil,                         z_order = 2},
  {time = 3.5,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
  {time = 3.25, image = nil,                         z_order = 2},
  {time = 3.0,  image = IMAGES.BOMNS[3],             z_order = 2},
  {time = 2.75, image = nil,                         z_order = 2},
  {time = 2.5,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
  {time = 2.25, image = nil,                         z_order = 2},
  {time = 2.0,  image = IMAGES.BOMNS[2],             z_order = 2},
  {time = 1.75, image = nil,                         z_order = 2},
  {time = 1.5,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
  {time = 1.25, image = nil,                         z_order = 2},
  {time = 1.0,  image = IMAGES.BOMNS[1],             z_order = 2},
  {time = 0.75, image = nil,                         z_order = 2},
  {time = 0.5,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
  {time = 0.4,  image = nil,                         z_order = 2},
  {time = 0.3,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
  {time = 0.2,  image = nil,                         z_order = 2},
  {time = 0.1,  image = IMAGES.BOMNS[#IMAGES.BOMNS], z_order = 2},
}

function Bomn:init(entities, pos, dude)
  Entity.init(self, entities, pos, 1, IMAGES.BOMNS[Bomn.TIME], 2)
  self.solid = false
  self.time = Bomn.TIME
  self.dude = dude
end

function Bomn:animate(backing, old_time)
  animate(self, backing, self.time, old_time, Bomn.ANIMATION)
end

function Bomn:explode(backing)
  self:kill(backing)

  local blasts = {}
  local to_blast = {}
  circle_contig(self.pos, Bomn.RADIUS, function(edge_pos)
    line(self.pos, edge_pos, function(pos)
      if not self.entities:valid_pos(pos)
          or self.entities:get_tile(pos) == TILES.WALL then
        return false
      end

      local key = pos_key(pos)
      if not blasts[key] then
        blasts[key] = pos
      end

      local other = self.entities:get_entity(pos)
      if other and other.solid then
        if not to_blast[key] then
          to_blast[key] = other
        end

        return pos_equal(pos, self.pos)
      end

      return not pos_equal(pos, edge_pos)
    end)
  end)

  for _, e in pairs(to_blast) do
    e:blasted(self, backing)
  end

  for _, p in pairs(blasts) do
    Blast(self.entities.level, p)
  end
end

function Bomn:bumped(dude, dude_backing)
  if self ~= dude.bomn then
    self:kill()
  end
  return true
end

function Bomn:l3_update(backing, elapsed)
  local old_time = self.time

  self.time = self.time - elapsed
  if self.time <= 0 then
    self:explode(backing)
    return
  end

  self:animate(backing, old_time)
end


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
  self.super_time = Super.TIME
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
  if     direction == "u" then dir = Pos( 0, -1)
  elseif direction == "d" then dir = Pos( 0,  1)
  elseif direction == "l" then dir = Pos(-1,  0)
  else                         dir = Pos( 1,  0) end

  local new_pos = Pos(self.pos.x + dir.x, self.pos.y + dir.y)
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
