local core = {}
package.loaded[...] = core


function core.Pos(x, y)
  return {x = x, y = y}
end

-- Seemed like a waste to use metatables and __eq here, since Pos objects are
-- so ubiquitous and checking them for equality so rare.
function core.pos_equal(a, b)
  return a.x == b.x and a.y == b.y
end

-- Returns a value suitable for using a Pos as a key in a table.
function core.pos_key(pos)
  return string.format("%x,%x", pos.x, pos.y)
end

function core.pos_add(a, b)
  return core.Pos(a.x + b.x, a.y + b.y)
end

-- In key presses, not as the crow flies.
function core.walk_dist(a, b)
  return math.abs(a.x - b.x) + math.abs(a.y - b.y)
end

-- As a bomn blast would travel.
function core.blast_dist(a, b)
  return math.floor(math.sqrt((a.x - b.x) ^ 2 + (a.y - b.y) ^ 2) + 0.5)
end

function core.Size(width, height)
  return {width = width, height = height}
end

function core.Rect(x, y, width, height)
  return {x = x, y = y, width = width, height = height}
end
