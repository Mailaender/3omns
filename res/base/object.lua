--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <http://chazomaticus.github.io/3omns/>
  base - base game logic for 3omns
  Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

  3omns is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  3omns is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  3omns.  If not, see <http://www.gnu.org/licenses/>.
]]

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
