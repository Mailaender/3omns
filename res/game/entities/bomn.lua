local obj    = require("object")
local Entity = require("entities.entity")

local Bomn = obj.class("Bomn", Entity)
package.loaded[...] = Bomn

local core   = require("core")
local util   = require("util")
local serial = require("serial")
local Blast  = require("sprites.blast")


Bomn.TIME = 3
Bomn.RADIUS = 8

-- TODO: move this into an init function.
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

function Bomn:init_base(entities, backing)
  Entity.init_base(self, entities, backing)

  self.solid   = false
  self.time    = Bomn.TIME
  self.dude_id = nil

  self:animate(backing)
end

function Bomn:init(entities, pos, dude_id)
  Entity.init(self, entities, pos, 1)

  self.dude_id = dude_id
end

function Bomn:serialize()
  return serial.serialize_number(self.time)
      .. serial.serialize_number(self.dude_id)
end

function Bomn:sync(serialized, start, backing)
  self.time,    start = serial.deserialize_number(serialized, start)
  self.dude_id, start = serial.deserialize_number(serialized, start)
  return start
end

function Bomn:animate(backing)
  local frame = util.get_animation_frame(Bomn.ANIMATION, self.time)
  self:set_image(frame.image, backing)
  self:set_z_order(frame.z_order, backing)
end

function Bomn:explode(backing)
  self:kill(backing)

  local blasts = {}
  local to_blast = {}
  util.circle_contig(self.pos, Bomn.RADIUS, function(edge_pos)
    util.line(self.pos, edge_pos, function(pos)
      if not self.entities:pos_valid(pos)
          or self.entities:get_tile(pos) == TILES.WALL then
        return false
      end

      local key = core.pos_key(pos)
      if not blasts[key] then
        blasts[key] = pos
      end

      local other = self.entities:get_entity(pos)
      if other and other.solid then
        if not to_blast[key] then
          to_blast[key] = other
        end

        return core.pos_equal(pos, self.pos)
      end

      return not core.pos_equal(pos, edge_pos)
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
  if self.dude_id ~= dude.id then
    self:kill()
  end
  return true
end

function Bomn:l3_update(backing, elapsed)
  self.time = self.time - elapsed
  if self.time <= 0 then
    self:explode(backing)
    return
  end

  self:animate(backing)
end
