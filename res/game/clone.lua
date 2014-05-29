local serial = require("serial")


-- TODO: some way of "versioning" these, so people have to be playing with
-- similar, if not identical, code.  This might be a good idea for the Lua
-- files themselves, not just for the data string.

local function serialize(entity)
  return serial.serialize_type(entity:get_type()) .. entity:serialize()
end


return {
  serialize = serialize,
}
