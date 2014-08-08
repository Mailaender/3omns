-- TODO: test agents (in C code, probably).


-- Always needed by l3_init().
L3_TILE_IMAGES = {}
L3_HEART_IMAGES = {}


local test_assert = assert
local f = "FAILED: "


test_assert(l3 ~= nil, f.."l3 defined")
test_assert(l3.RESOURCE_PATH ~= nil, f.."l3.RESOURCE_PATH defined")
test_assert(l3.GAME ~= nil, f.."l3.GAME defined")
test_assert(l3.CLIENT ~= nil, f.."l3.CLIENT defined")
test_assert(l3.DEBUG ~= nil, f.."l3.DEBUG defined")

-- TODO: test l3.images (need window in driver.c).

local l = l3.level.new({width = 10, height = 11}, 123)

test_assert(l:get_size().width == 10, f.."level width")
test_assert(l:get_size().height == 11, f.."level height")

l:set_tile({x = 2, y = 3}, 234)
test_assert(l:get_tile({x = 1, y = 1}) == 0, f.."unset tile value")
test_assert(l:get_tile({x = 2, y = 3}) == 234, f.."set tile value")

local s = l:new_sprite({})

test_assert(s:get_pos().x == 1, f.."initial sprite x pos")
test_assert(s:get_pos().y == 1, f.."initial sprite y pos")
s:set_pos({x = 4, y = 5})
test_assert(s:get_pos().x == 4, f.."set sprite x pos")
test_assert(s:get_pos().y == 5, f.."set sprite y pos")

s:destroy()

local e_ctx = {}
local e = l:new_entity(e_ctx)
local e_id = e:get_id()
l:set_dude(1, e_id)

test_assert(l:get_entity(e_id):get_id() == e_id, f.."fetched entity matches")

test_assert(e:get_context() == e_ctx, f.."entity returned initial context")
local e_ctx2 = {}
e:set_context(e_ctx2)
test_assert(e:get_context() == e_ctx2, f.."entity returned set context")

test_assert(e:get_pos().x == 1, f.."initial entity x pos")
test_assert(e:get_pos().y == 1, f.."initial entity y pos")
e:set_pos({x = 6, y = 7})
test_assert(e:get_pos().x == 6, f.."set entity x pos")
test_assert(e:get_pos().y == 7, f.."set entity y pos")

test_assert(e:get_life() == 0, f.."initial entity life")
e:set_life(23)
test_assert(e:get_life() == 23, f.."set entity life")
