local util  = require("util")
local Super = require("entities.super")
local Crate = require("entities.crate")
local Bomn  = require("entities.bomn")
local Dude  = require("entities.dude")


local deserialize_map = {
  S = Super,
  C = Crate,
  B = Bomn,
  D = Dude,
}
local serialize_map = util.invert_table(deserialize_map)

local function serialize_type(t)
  return serialize_map[t] or ' '
end

local function deserialize_type(s, start)
  start = start or 1

  return deserialize_map[string.sub(s, start, start)], start + 1
end

local function serialize_number(n)
  return n and string.format("<%a>", n) or "<>"
end

local function deserialize_number(s, start)
  start = start or 1

  local n = string.match(s, "^<([^>]-)>", start)
  assert(n, "Couldn't match a number")
  return tonumber(n), start + string.len(n) + 2
end


return {
  serialize_type     = serialize_type,
  deserialize_type   = deserialize_type,
  serialize_number   = serialize_number,
  deserialize_number = deserialize_number,
}
