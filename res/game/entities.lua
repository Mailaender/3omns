local function new(class, ...)
  local self = setmetatable({}, class)
  self:init(...)
  return self
end

local function is_a(object, class)
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


Entities = class()

function Entities:init(level)
  self.level = level
  self.index = {}
end

-- The C entities are referred to as the "backing" for Lua entities.  Deal.
function Entities:new_backing(entity)
  local backing = self.level:new_entity(entity)
  return backing:get_id(), backing
end

function Entities:get_backing(id)
  return self.level:get_entity(id)
end

local function entities_index_key(pos)
  return string.format("%x,%x", pos.x, pos.y)
end

function Entities:get_entity(pos)
  return self.index[entities_index_key(pos)]
end

function Entities:move(entity, old_pos)
  if old_pos then
    self.index[entities_index_key(old_pos)] = nil
  end

  local key = entities_index_key(entity.pos)
  -- FIXME: this won't work once we allow players to stand on their own bomns.
  assert(not self.index[key], "Two entities can't occupy the same space")
  self.index[key] = entity
end

function Entities:remove(entity)
  self.index[entities_index_key(entity.pos)] = nil
end


local Entity = class()

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


local Crate = class(Entity)

function Crate:init(entities, pos)
  Entity.init(self, entities, pos, 1, IMAGES.CRATE)
  -- TODO
end


local Dude = class(Entity)

function Dude:init(entities, pos, player)
  Entity.init(self, entities, pos, 10, IMAGES.DUDES[player])
  self.player = player
end

function Dude:l3_update(backing, elapsed)
  -- TODO
end


local public = {
  Crate = Crate,
  Dude  = Dude,
}

for name, constructor in pairs(public) do
  Entities[name] = function(self, ...)
    return constructor(self, ...)
  end
end
