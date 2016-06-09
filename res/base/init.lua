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

package.path = string.format(
  "%s/%s/?.lua;%s/%s/?/?.lua;%s",
  l3.RESOURCE_PATH,
  l3.GAME,
  l3.RESOURCE_PATH,
  l3.GAME,
  package.path
)

local core = require("core")
local generate = require("generate")
local sync = require("sync")


local function init()
  if not core.init() then
    return false
  end

  -- Set some (global) names that l3 requires.
  L3_TILE_IMAGES = {
    [core.TILES.BLANK] = core.IMAGES.BLANK,
    [core.TILES.WALL] = core.IMAGES.WALL,
  }
  L3_BORDER_IMAGE = core.IMAGES.WALL
  L3_HEART_IMAGES = core.IMAGES.HEARTS

  l3_generate = generate.generate

  l3_sync = sync.sync
  l3_sync_deleted = sync.sync_deleted

  return true
end

-- TODO: it might be more awesome to have this file return the function
-- (without invoking it) and let the C code call it at its leisure (before
-- anything else is done in lua, obvz).  Sadly, as rad as that would be, I
-- can't see how it would actually be useful, so I haven't done it.
init()
