local core = require("core")
local obj  = require("object")


local Entity = obj.class()

function Entity:init(entities, pos, life, image, z_order)
  self.entities = entities
  local backing
  self.id, backing = entities:new_backing(self)
  self:set_pos(pos, backing)
  self:set_life(life, backing)
  self:set_image(image, backing)
  self:set_z_order(z_order, backing)
  self.solid = true -- Whether the entity blocks blasts and player movement.
end

Entity.is_a = obj.is_a

function Entity:get_backing()
  return self.entities:get_backing(self.id)
end

function Entity:exists()
  return self:get_backing() ~= nil
end

function Entity:set_pos(pos, backing)
  if self.pos == nil or not core.pos_equal(pos, self.pos) then
    backing = backing or self:get_backing()
    local old = self.pos

    self.pos = pos
    backing:set_pos(pos)

    self.entities:move(self, old)
  end

  return self
end

function Entity:set_life(life, backing)
  if life ~= self.life then
    backing = backing or self:get_backing()

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
  if image ~= self.image then
    backing = backing or self:get_backing()

    self.image = image
    backing:set_image(image)
  end

  return self
end

function Entity:set_z_order(z_order, backing)
  if z_order ~= self.z_order then
    backing = backing or self:get_backing()

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
  return not self.solid
end

function Entity:blasted(bomn, bomn_backing)
  if self.solid then
    self:kill()
  end
end


return Entity
