AM_CPPFLAGS = -Wall -I $(top_srcdir)

noinst_LIBRARIES = libn3.a
libn3_a_SOURCES = n3.h core.c n3.c


check_PROGRAMS = tests/core

tests_core_SOURCES = tests/test.h tests/core.c
tests_core_LDADD = libn3.a ../b3/libb3.a


TESTS = $(check_PROGRAMS)
