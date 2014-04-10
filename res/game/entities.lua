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

function Entities:new_entity(entity)
  return self.level:new_entity(entity)
end

function Entities:move(entity, old_pos)
  -- TODO
end

function Entities:remove(entity)
  -- TODO
end


local Entity = class()

function Entity:is_a(class)
  return is_a(self, class)
end

function Entity:init(entities, pos, life, image)
  self.entities = entities
  self.backing = entities:new_entity(self)
  self:set_pos(pos)
  self:set_life(life)
  self:set_image(image)
end

function Entity:set_pos(pos)
  if self.pos == nil or not pos_equal(pos, self.pos) then
    local old = self.pos

    self.pos = pos
    self.backing:set_pos(pos)

    self.entities:move(self, old)
  end
  return self
end

function Entity:set_life(life)
  if life ~= self.life then
    self.life = life
    self.backing:set_life(life)

    if life == 0 then
      self.entities:remove(self)
    end
  end
  return self
end

function Entity:set_image(image)
  if image ~= self.image then
    self.image = image
    self.backing:set_image(image)
  end
  return self
end

function Entity:kill()
  self:set_life(0)
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

function Dude:l3_update(entity, elapsed)
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
