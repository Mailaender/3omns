local serial = {}
package.loaded[...] = serial

local util     = require("util")
local Entities = require("entities")


local deserialize_map = {
  S = Entities.Super,
  C = Entities.Crate,
  B = Entities.Bomn,
  D = Entities.Dude,
}
local serialize_map = util.invert_table(deserialize_map)

function serial.serialize_type(t)
  return serialize_map[t] or ' '
end

function serial.deserialize_type(s, start)
  start = start or 1

  return deserialize_map[string.sub(s, start, start)], start + 1
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
