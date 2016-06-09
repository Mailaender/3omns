--[[
  3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
          <https://chazomaticus.github.io/3omns/>
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

local core = {}
package.loaded[...] = core


core.IMAGES = nil
core.TILES = nil

function core.init()
  if core.IMAGES then
    return false
  end

  local seed = os.time()
  math.randomseed(seed)

  if not l3.CLIENT then
    core.debug_print("Seed: " .. seed)
  end

  local sprites = l3.image.load(core.resource("/gfx/sprites.png"))
  local TILE_SIZE = 16

  local function SpriteRect(x, y)
    return core.Rect(x, y, TILE_SIZE, TILE_SIZE)
  end

  core.IMAGES = {
    BLANK = sprites:sub(SpriteRect( 0, 0)),
    WALL  = sprites:sub(SpriteRect(16, 0)),
    CRATE = sprites:sub(SpriteRect(32, 0)),
    SUPER = sprites:sub(SpriteRect(48, 0)),
    DUDES = {
      sprites:sub(SpriteRect( 0, 16)),
      sprites:sub(SpriteRect(16, 16)),
      sprites:sub(SpriteRect(32, 16)),
      sprites:sub(SpriteRect(48, 16)),
    },
    SUPER_DUDES = {
      sprites:sub(SpriteRect( 0, 32)),
      sprites:sub(SpriteRect(16, 32)),
      sprites:sub(SpriteRect(32, 32)),
      sprites:sub(SpriteRect(48, 32)),
    },
    HEARTS = {
      sprites:sub(SpriteRect( 0, 48)),
      sprites:sub(SpriteRect(16, 48)),
      sprites:sub(SpriteRect(32, 48)),
      sprites:sub(SpriteRect(48, 48)),
    },
    BLASTS = {
      sprites:sub(SpriteRect( 0, 64)),
      sprites:sub(SpriteRect(16, 64)),
      sprites:sub(SpriteRect(32, 64)),
      sprites:sub(SpriteRect(48, 64)),
      sprites:sub(SpriteRect( 0, 80)),
      sprites:sub(SpriteRect(16, 80)),
      sprites:sub(SpriteRect(32, 80)),
      sprites:sub(SpriteRect(48, 80)),
    },
    BOMNS = {
      sprites:sub(SpriteRect(64, 16)),
      sprites:sub(SpriteRect(64, 32)),
      sprites:sub(SpriteRect(64, 48)),
      sprites:sub(SpriteRect(64, 64)),
      sprites:sub(SpriteRect(64, 80)),
      sprites:sub(SpriteRect(64,  0)), -- Last one is generic, not labeled.
    },
  }

  core.TILES = {
    BLANK = string.byte(" "),
    WALL = string.byte("X"),
  }

  return true
end

function core.debug_print(...)
  if l3.DEBUG then
    print(...)
  end
end

function core.resource(filename)
  return string.format("%s%s", l3.RESOURCE_PATH, filename)
end

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
