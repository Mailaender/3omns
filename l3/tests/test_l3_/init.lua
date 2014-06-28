-- TODO: test agents (in C code, probably).


-- Always needed by l3_init().
L3_TILE_IMAGES = {}
L3_HEART_IMAGES = {}


local test_assert = assert


test_assert(l3 ~= nil)
test_assert(l3.RESOURCE_PATH ~= nil)
test_assert(l3.GAME ~= nil)
test_assert(l3.CLIENT ~= nil)
test_assert(l3.DEBUG ~= nil)

-- TODO: test l3.images (need window in driver.c).

local l = l3.level.new({width = 10, height = 11}, 123)

test_assert(l:get_size().width == 10)
test_assert(l:get_size().height == 11)

l:set_tile({x = 2, y = 3}, 234)
test_assert(l:get_tile({x = 1, y = 1}) == 0)
test_assert(l:get_tile({x = 2, y = 3}) == 234)

local s = l:new_sprite({})

test_assert(s:get_pos().x == 1)
test_assert(s:get_pos().y == 1)
s:set_pos({x = 4, y = 5})
test_assert(s:get_pos().x == 4)
test_assert(s:get_pos().y == 5)

s:destroy()

local e_ctx = {}
local e = l:new_entity(e_ctx)
local e_id = e:get_id()
l:set_dude(1, e_id)

test_assert(l:get_entity(e_id):get_id() == e_id)

test_assert(e:get_context() == e_ctx)
local e_ctx2 = {}
e:set_context(e_ctx2)
test_assert(e:get_context() == e_ctx2)

test_assert(e:get_pos().x == 1)
test_assert(e:get_pos().y == 1)
e:set_pos({x = 6, y = 7})
test_assert(e:get_pos().x == 6)
test_assert(e:get_pos().y == 7)

test_assert(e:get_life() == 0)
e:set_life(23)
test_assert(e:get_life() == 23)
