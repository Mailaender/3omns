--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <http://chazomaticus.github.io/3omns/>
  base - base game logic for 3omns
  Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

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

local serial = {}
package.loaded[...] = serial

local util     = require("util")
local Entities = require("entities")


local serialize_map
local deserialize_map

local function get_deserialize_map()
  if not deserialize_map then
    deserialize_map = {
      S = Entities.Super,
      C = Entities.Crate,
      B = Entities.Bomn,
      D = Entities.Dude,
    }
  end

  return deserialize_map
end

local function get_serialize_map()
  if not serialize_map then
    serialize_map = util.invert_table(get_deserialize_map())
  end

  return serialize_map
end

function serial.serialize_type(t)
  return get_serialize_map()[t] or ' '
end

function serial.deserialize_type(s, start)
  start = start or 1

  return get_deserialize_map()[string.sub(s, start, start)], start + 1
end

function serial.serialize_number(n)
  return n and string.format("<%a>", n) or "<>"
end

function serial.deserialize_number(s, start)
  start = start or 1

  local n = string.match(s, "^<([^>]-)>", start)
  assert(n, "Couldn't match a number")
  return tonumber(n), start + string.len(n) + 2
end
