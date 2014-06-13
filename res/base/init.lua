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

package.path = string.format(
  "%s/%s/?.lua;%s/%s/?/?.lua;%s",
  l3.RESOURCE_PATH,
  l3.GAME,
  l3.RESOURCE_PATH,
  l3.GAME,
  package.path
)

local core = require("core")
local util = require("util")


local seed = os.time()
math.randomseed(seed)
util.debug_print("Seed: " .. seed)

do
  local sprites = l3.image.load(util.resource("/gfx/sprites.png"))
  local TILE_SIZE = 16

  local function SpriteRect(x, y)
    return core.Rect(x, y, TILE_SIZE, TILE_SIZE)
  end

  IMAGES = {
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
end

TILES = {
  BLANK = string.byte(" "),
  WALL  = string.byte("X"),
}

L3_TILE_IMAGES = {
  [TILES.BLANK] = IMAGES.BLANK,
  [TILES.WALL]  = IMAGES.WALL,
}
L3_BORDER_IMAGE = IMAGES.WALL
L3_HEART_IMAGES = IMAGES.HEARTS

l3_generate = require("generate").generate

local sync = require("sync")
l3_sync = sync.sync
l3_sync_deleted = sync.sync_deleted