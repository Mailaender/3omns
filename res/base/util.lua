local util = {}
package.loaded[...] = util

local core = require("core")


function util.debug_print(string)
  if l3.DEBUG then
    print(string)
  end
end

function util.resource(filename)
  return string.format("%s%s", l3.RESOURCE_PATH, filename)
end

function util.invert_table(t)
  local r = {}
  for k, v in pairs(t) do r[v] = k end
  return r
end

-- Stolen from the !w Bresenham's line algorithm page.  Goes from a to b (and
-- beyond, as limited by the callback function returning false), calling the
-- callback for each pos on the line.
function util.line(a, b, callback)
  local dx = math.abs(b.x - a.x)
  local dy = math.abs(b.y - a.y)
  local sx = a.x < b.x and 1 or -1
  local sy = a.y < b.y and 1 or -1
  local err = dx - dy

  local x = a.x
  local y = a.y
  while callback(core.Pos(x, y)) do
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
function util.circle_contig(center, radius, callback)
  local function octants(x, y)
    callback(core.Pos(center.x + x, center.y + y))
    callback(core.Pos(center.x + y, center.y + x))
    callback(core.Pos(center.x - x, center.y + y))
    callback(core.Pos(center.x - y, center.y + x))
    callback(core.Pos(center.x - x, center.y - y))
    callback(core.Pos(center.x - y, center.y - x))
    callback(core.Pos(center.x + x, center.y - y))
    callback(core.Pos(center.x + y, center.y - x))
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

-- Animation helper for sprites or entities.
function util.get_animation_frame(animation, time)
  -- Note that time counts down for these objects.  It's more of a "lifetime
  -- remaining".  The animation frames must be ordered first (biggest time) to
  -- last (lowest time).

  for i = 1, #animation do
    if animation[i].time >= time
        and (not animation[i + 1] or animation[i + 1].time < time) then
      return animation[i]
    end
  end
  return nil
end
