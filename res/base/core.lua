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
