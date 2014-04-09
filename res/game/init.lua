math.randomseed(os.time())

dofile(l3.RESOURCE_PATH .. "/game/defs.lua")
dofile(l3.RESOURCE_PATH .. "/game/entities.lua")
dofile(l3.RESOURCE_PATH .. "/game/generate.lua")


do
  local sprites = l3.image.load(l3.RESOURCE_PATH .. "/gfx/sprites.png")
  local TILE_SIZE = 16

  local function SpriteRect(x, y)
    return Rect(x, y, TILE_SIZE, TILE_SIZE)
  end

  IMAGES = {
    BLANK = sprites:sub(SpriteRect( 0, 0)),
    WALL  = sprites:sub(SpriteRect(16, 0)),
    CRATE = sprites:sub(SpriteRect(32, 0)),
    DUDES = {
      sprites:sub(SpriteRect( 0, 16)),
      sprites:sub(SpriteRect(16, 16)),
      sprites:sub(SpriteRect(32, 16)),
      sprites:sub(SpriteRect(48, 16)),
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
