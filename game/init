do
  local sprites = l3.image.load("gfx/sprites.png")
  local function sprite_rect(x, y)
    local tile_size = 16
    return {x=x, y=y, width=tile_size, height=tile_size}
  end

  IMAGES = {
    BLUE = sprites:sub(sprite_rect(16, 16)),
    BOMN = sprites:sub(sprite_rect(64, 0)),
  }
end

function draw_sprites()
  IMAGES.BLUE:draw(10, 10)
  IMAGES.BOMN:draw(30, 30)
end
