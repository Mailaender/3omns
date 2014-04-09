local MAP_SIZE = Size(30, 30)
local MAX_ENTITIES = MAP_SIZE.width * MAP_SIZE.height * 2

local function generate_spawns(ctx)
  local quads = {
    Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2),
    Pos(0,                  0),
    Pos(MAP_SIZE.width / 2, 0),
    Pos(0,                  MAP_SIZE.height / 2),
  }

  for i, q in ipairs(quads) do
    ctx.spawns[i] = Pos(
      math.random(q.x + 2, q.x + MAP_SIZE.width  / 2 - 2),
      math.random(q.y + 2, q.y + MAP_SIZE.height / 2 - 2)
    )
  end
end

local function wall_grid(ctx, center)
  local walls = {}
  for x = center.x - 3, center.x + 3, 3 do
    for y = center.y - 3, center.y + 3, 3 do
      if ctx.level:get_tile(Pos(x, y)) == 0 then
        walls[#walls + 1] = Pos(x, y)
      end
    end
  end

  while #walls > 5 do
    table.remove(walls, math.random(#walls))
  end

  for _, w in ipairs(walls) do
    ctx.level:set_tile(w, TILES.WALL)
  end
end

local function generate_walls(ctx)
  local fourth = Size(
    math.floor(MAP_SIZE.width  / 4),
    math.floor(MAP_SIZE.height / 4)
  )
  local pos = {
    Pos(fourth.width,                  fourth.height),
    Pos(fourth.width,                  MAP_SIZE.height - fourth.height),
    Pos(MAP_SIZE.width - fourth.width, fourth.height),
    Pos(MAP_SIZE.width - fourth.width, MAP_SIZE.height - fourth.height),
  }

  for _, p in ipairs(pos) do
    wall_grid(ctx, p)
  end
  wall_grid(ctx, Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2))
end

local function fill_space(ctx)
  for x = 1, MAP_SIZE.width do
    for y = 1, MAP_SIZE.height do
      if ctx.level:get_tile(Pos(x, y)) == 0 then
        ctx.level:set_tile(Pos(x, y), TILES.BLANK)
      end
    end
  end
end

local function spawn_entities(ctx)
  for i = 1, 4 do
    ctx.dudes[i] = Dude(ctx.level, ctx.spawns[i], i)
  end
end

function l3_generate()
  local ctx = {
    level = l3.level.new(MAP_SIZE, MAX_ENTITIES),
    spawns = {},
    dudes = {},
  }

  generate_spawns(ctx)
  generate_walls(ctx)
  fill_space(ctx)
  spawn_entities(ctx)

  return ctx.level
end
