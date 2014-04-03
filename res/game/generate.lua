local MAP_WIDTH = 30
local MAP_HEIGHT = 30

local function generate_spawns(ctx)
  local x_mins = {MAP_WIDTH / 2, 0, MAP_WIDTH / 2, 0}
  local y_mins = {MAP_HEIGHT / 2, 0, 0, MAP_HEIGHT / 2}

  for i = 1, 4 do
    local spawn_x = math.random(x_mins[i] + 2, x_mins[i] + MAP_WIDTH / 2 - 2)
    local spawn_y = math.random(y_mins[i] + 2, y_mins[i] + MAP_HEIGHT / 2 - 2)
    ctx.spawns[i] = {x=spawn_x, y=spawn_y}
  end
end

local function wall_grid(ctx, center_x, center_y)
  local walls = {}
  for x = center_x - 3, center_x + 3, 3 do
    for y = center_y - 3, center_y + 3, 3 do
      if ctx.map:get_tile(x, y) == 0 then
        walls[#walls + 1] = {x=x, y=y}
      end
    end
  end

  while #walls > 5 do
    table.remove(walls, math.random(#walls))
  end

  for _, w in pairs(walls) do
    ctx.map:set_tile(w.x, w.y, TILES.WALL)
  end
end

local function generate_walls(ctx)
  local quarter_width = math.floor(MAP_WIDTH / 4)
  local quarter_height = math.floor(MAP_HEIGHT / 4)

  for _, x in pairs({quarter_width, MAP_WIDTH - quarter_width}) do
    for _, y in pairs({quarter_height, MAP_HEIGHT - quarter_height}) do
      wall_grid(ctx, x, y)
    end
  end
  wall_grid(ctx, MAP_WIDTH / 2, MAP_HEIGHT / 2)
end

local function fill_space(ctx)
  for x = 1, MAP_WIDTH do
    for y = 1, MAP_HEIGHT do
      if ctx.map:get_tile(x, y) == 0 then
        ctx.map:set_tile(x, y, TILES.BLANK)
      end
    end
  end
end

local function spawn_entities(ctx)
  -- TODO
end

function generate()
  local ctx = {
    map = l3.map.new(MAP_WIDTH, MAP_HEIGHT),
    spawns = {},
  }

  generate_spawns(ctx)
  generate_walls(ctx)
  fill_space(ctx)
  spawn_entities(ctx)

  return ctx.map
end