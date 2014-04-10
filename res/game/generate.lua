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

-- Stolen from the !w Bresenham's line algorithm page.
local function line_to_edge(a, b, callback)
  local dx = math.abs(b.x - a.x)
  local dy = math.abs(b.y - a.y)
  local sx = a.x < b.x and 1 or -1
  local sy = a.y < b.y and 1 or -1
  local err = dx - dy

  local x = a.x
  local y = a.y
  while x > 0 and x <= MAP_SIZE.width and y > 0 and y <= MAP_SIZE.height do
    callback(Pos(x, y))
    local e2 = err * 2
    if e2 > -dy then
      err = err - dy
      x = x + sx
    end
    if e2 < dx then
      err = err + dx
      y = y + sy
    end
  end
end

local function spawn_crates(ctx)
  local direction
  local function crates(pos)
    local start = Pos(
      pos.x + direction.x * math.random(-2, -1),
      pos.y + direction.y * math.random(-2, -1)
    )

    for i = 1, 4 do
      -- TODO: make sure there aren't any other entities there.
      ctx.entities:Crate(
        Pos(start.x + direction.x * i, start.y + direction.y * i)
      )
    end
  end

  local center = Pos(MAP_SIZE.width / 2, MAP_SIZE.height / 2)
  local bisect

  bisect = Pos(
    math.floor((ctx.spawns[1].x + ctx.spawns[3].x) / 2 + 0.5),
    math.floor((ctx.spawns[1].y + ctx.spawns[3].y) / 2 + 0.5)
  )
  direction = Pos(0, 1)
  line_to_edge(center, bisect, crates)

  bisect = Pos(
    math.floor((ctx.spawns[2].x + ctx.spawns[4].x) / 2 + 0.5),
    math.floor((ctx.spawns[2].y + ctx.spawns[4].y) / 2 + 0.5)
  )
  direction = Pos(0, 1)
  line_to_edge(center, bisect, crates)

  bisect = Pos(
    math.floor((ctx.spawns[2].x + ctx.spawns[3].x) / 2 + 0.5),
    math.floor((ctx.spawns[2].y + ctx.spawns[3].y) / 2 + 0.5)
  )
  direction = Pos(1, 0)
  line_to_edge(center, bisect, crates)

  bisect = Pos(
    math.floor((ctx.spawns[1].x + ctx.spawns[4].x) / 2 + 0.5),
    math.floor((ctx.spawns[1].y + ctx.spawns[4].y) / 2 + 0.5)
  )
  direction = Pos(1, 0)
  line_to_edge(center, bisect, crates)
end

local function spawn_dudes(ctx)
  for i, s in ipairs(ctx.spawns) do
    ctx.dudes[i] = ctx.entities:Dude(s, i)
  end
end

function l3_generate()
  local level = l3.level.new(MAP_SIZE, MAX_ENTITIES)
  local ctx = {
    level = level,
    entities = Entities(level),
    spawns = {},
    dudes = {},
  }

  generate_spawns(ctx)
  generate_walls(ctx)
  fill_space(ctx)
  spawn_crates(ctx)
  spawn_dudes(ctx)

  return ctx.level
end
