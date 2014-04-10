function Pos(x, y)
  return {x = x, y = y}
end

-- Seemed like a waste to use metatables and __eq here, since Pos objects are
-- so ubiquitous and checking them for equality so rare.
function pos_equal(a, b)
  return a.x == b.x and a.y == b.y
end

function Size(width, height)
  return {width = width, height = height}
end

function Rect(x, y, width, height)
  return {x = x, y = y, width = width, height = height}
end
