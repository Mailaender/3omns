local obj = {}
package.loaded[...] = obj


function obj.create(class)
  return setmetatable({}, class)
end

local function new(class, ...)
  local self = obj.create(class)
  self:init(...)
  return self
end

function obj.get_type(object)
  return getmetatable(object)
end

-- TODO: an is_a function that handles inheritance.

function obj.class(name, parent)
  local c = {}
  c.__index = c
  c.class_name = name

  setmetatable(c, {
    __index = parent,
    __call = new,
  })

  return c
end
