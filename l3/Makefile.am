AM_CPPFLAGS = -Wall -I $(top_srcdir) $(LUA_CFLAGS)

noinst_LIBRARIES = libl3.a
libl3_a_SOURCES = agent.c image.c internal.h l3.c l3.h level.c sync.c util.c


check_PROGRAMS = tests/driver

tests_driver_SOURCES = tests/driver.c
tests_driver_LDADD = libl3.a ../b3/libb3.a $(SDL_LIBS) $(LUA_LIBS)


TESTS = tests/test_l3

dist_check_DATA = tests/run $(TESTS) $(TESTS:%=%_res/init.lua)
