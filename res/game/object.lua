local function create(class)
  return setmetatable({}, class)
end

local function new(class, ...)
  local self = create(class)
  self:init(...)
  return self
end

local function get_type(object)
  return getmetatable(object)
end

-- TODO: an is_a function that handles inheritance.

local function class(name, parent)
  local c = {}
  c.__index = c
  c.class_name = name

  setmetatable(c, {
    __index = parent,
    __call = new,
  })

  return c
end


return {
  create   = create,
  get_type = get_type,
  class    = class,
}
