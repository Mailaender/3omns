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


return {
  is_a  = is_a,
  class = class,
}
