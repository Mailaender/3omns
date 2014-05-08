local function Pos(x, y)
  return {x = x, y = y}
end

-- Seemed like a waste to use metatables and __eq here, since Pos objects are
-- so ubiquitous and checking them for equality so rare.
local function pos_equal(a, b)
  return a.x == b.x and a.y == b.y
end

-- Returns a value suitable for using a Pos as a key in a table.
local function pos_key(pos)
  return string.format("%x,%x", pos.x, pos.y)
end

local function pos_add(a, b)
  return Pos(a.x + b.x, a.y + b.y)
end

-- In key presses, not as the crow flies.
local function walk_dist(a, b)
  return math.abs(a.x - b.x) + math.abs(a.y - b.y)
end

-- As a bomn blast would travel.
local function blast_dist(a, b)
  return math.floor(math.sqrt((a.x - b.x) ^ 2 + (a.y - b.y) ^ 2) + 0.5)
end

local function Size(width, height)
  return {width = width, height = height}
end

local function Rect(x, y, width, height)
  return {x = x, y = y, width = width, height = height}
end


return {
  Pos        = Pos,
  pos_equal  = pos_equal,
  pos_key    = pos_key,
  pos_add    = pos_add,
  walk_dist  = walk_dist,
  blast_dist = blast_dist,
  Size       = Size,
  Rect       = Rect,
}
