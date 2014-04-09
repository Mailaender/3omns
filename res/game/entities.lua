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

local function new_l3_entity(self, level, pos, life, image)
  local e = level:new_entity(self)
  e:set_pos(pos.x, pos.y)
  e:set_life(life)
  e:set_image(image)
  return e
end


Entity = class()

function Entity:init(level, pos, life, image)
  self.pos = pos
  self.life = life
  self.image = image

  new_l3_entity(self, level, pos, life, image)
end

function Entity:is_a(class)
  return is_a(self, class)
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
