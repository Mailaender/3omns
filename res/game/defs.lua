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

-- Stolen from the !w Bresenham's line algorithm page.  Goes from a to b (and
-- beyond, as limited by the callback function returning false), calling the
-- callback for each pos on the line.
function line(a, b, callback)
  local dx = math.abs(b.x - a.x)
  local dy = math.abs(b.y - a.y)
  local sx = a.x < b.x and 1 or -1
  local sy = a.y < b.y and 1 or -1
  local err = dx - dy

  local x = a.x
  local y = a.y
  while callback(Pos(x, y)) do
    local e2 = err * 2
    if e2 > -dy then
      err = err - dy
      x = x + sx
    end
    if e2 < dx then
      err = err + dx
      y = y + sy
    end
  end
end

-- Stolen/modified from the !w midpoint circle algorithm page.  This selects
-- some extra points to make sure every point is adjacent (not just diagonally)
-- to another, i.e. the line is thicker on the jagged bits.
function circle_contig(center, radius, callback)
  local function octants(x, y)
    callback(Pos(center.x + x, center.y + y))
    callback(Pos(center.x + y, center.y + x))
    callback(Pos(center.x - x, center.y + y))
    callback(Pos(center.x - y, center.y + x))
    callback(Pos(center.x - x, center.y - y))
    callback(Pos(center.x - y, center.y - x))
    callback(Pos(center.x + x, center.y - y))
    callback(Pos(center.x + y, center.y - x))
  end

  local err = 1 - radius

  local x = radius
  local y = 0
  while x >= y do
    octants(x, y)

    y = y + 1
    if err < 0 then
      err = err + 2 * y + 1
    else
      x = x - 1
      err = err + 2 * (y - x + 1)

      octants(x, y - 1) -- Add the point making the line contiguous.
    end
  end
end
