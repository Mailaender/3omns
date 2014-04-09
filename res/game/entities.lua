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


Entity = class()

function Entity:is_a(class)
  return is_a(self, class)
end

function Entity:init(level, pos, life, image)
  self.backing = level:new_entity(self)
  self:set_pos(pos)
  self:set_life(life)
  self:set_image(image)
end

function Entity:set_pos(pos)
  self.pos = pos
  self.backing:set_pos(pos.x, pos.y)
end

function Entity:set_life(life)
  self.life = life
  self.backing:set_life(life)
end

function Entity:set_image(image)
  self.image = image
  self.backing:set_image(image)
end

function Entity:kill()
  self:set_life(0)
end


Crate = class(Entity)

function Crate:init(level, pos)
  Entity.init(self, level, pos, 1, IMAGES.CRATE)
  -- TODO
end


Dude = class(Entity)

function Dude:init(level, pos, player)
  Entity.init(self, level, pos, 10, IMAGES.DUDES[player])
  self.player = player
end

function Dude:l3_update(entity, elapsed)
  -- TODO
end
