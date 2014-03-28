function rect(x, y, width, height)
  return {x=x, y=y, width=width, height=height}
end

do
  local sprites = l3.image.load(l3.RESOURCE_PATH .. "/gfx/sprites.png")

  local function sprite_rect(x, y)
    local tile_size = 16
    return rect(x, y, tile_size, tile_size)
  end

  IMAGES = {
    BLANK = sprites:sub(sprite_rect(0, 0)),
    WALL = sprites:sub(sprite_rect(16, 0)),
  }
end

TILES = {
  BLANK           = string.byte(" "),
  WALL            = string.byte("X"),
  RED_SPAWN       = string.byte("r"),
  BLUE_SPAWN      = string.byte("b"),
  VIOLET_SPAWN    = string.byte("v"),
  GREEN_SPAWN     = string.byte("g"),
}

TILE_IMAGES = {
  [TILES.BLANK] = IMAGES.BLANK,
  [TILES.WALL] = IMAGES.WALL,
}

math.randomseed(os.time())

dofile(l3.RESOURCE_PATH .. "/game/map.lua")
