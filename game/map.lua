local MAP_WIDTH = 30
local MAP_HEIGHT = 30

local function generate_spawns(map_ctx)
  local x_mins = {MAP_WIDTH / 2, 0, MAP_WIDTH / 2, 0}
  local y_mins = {MAP_HEIGHT / 2, 0, 0, MAP_HEIGHT / 2}
  local spawns = {
    TILES.RED_SPAWN,
    TILES.BLUE_SPAWN,
    TILES.VIOLET_SPAWN,
    TILES.GREEN_SPAWN,
  }

  for i = 1, 4 do
    local spawn_x = math.random(x_mins[i] + 2, x_mins[i] + MAP_WIDTH / 2 - 2)
    local spawn_y = math.random(y_mins[i] + 2, y_mins[i] + MAP_HEIGHT / 2 - 2)
    map_ctx[spawns[i]] = {x=spawn_x, y=spawn_y}

    map_ctx.map:set_tile(spawn_x, spawn_y, spawns[i])
  end
end

local function wall_grid(map_ctx, center_x, center_y)
  local walls = {}
  for x = center_x - 3, center_x + 3, 3 do
    for y = center_y - 3, center_y + 3, 3 do
      if map_ctx.map:get_tile(x, y) == 0 then
        walls[#walls + 1] = {x=x, y=y}
      end
    end
  end

  while #walls > 5 do
    table.remove(walls, math.random(#walls))
  end

  for _, w in pairs(walls) do
    map_ctx.map:set_tile(w.x, w.y, TILES.WALL)
  end
end

local function generate_walls(map_ctx)
  for x = MAP_WIDTH / 4, MAP_WIDTH / 4 + MAP_WIDTH / 2, MAP_WIDTH / 2 do
    for y = MAP_HEIGHT / 4, MAP_HEIGHT / 4 + MAP_HEIGHT / 2, MAP_HEIGHT / 2 do
      wall_grid(map_ctx, x, y)
    end
  end
  wall_grid(map_ctx, MAP_WIDTH / 2, MAP_HEIGHT / 2)
end

function generate_map()
  local map_ctx = {}
  map_ctx.map = l3.map.new(MAP_WIDTH, MAP_HEIGHT)

  generate_spawns(map_ctx)
  generate_walls(map_ctx)

  return map_ctx.map
end
