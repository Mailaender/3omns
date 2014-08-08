AM_CPPFLAGS = -Wall -I $(top_srcdir)

noinst_LIBRARIES = libn3.a
libn3_a_SOURCES = buffer.c internal.h n3.c n3.h ordered_list.h proto.c raw.c


check_PROGRAMS = tests/test_raw

tests_test_raw_SOURCES = tests/test.h tests/test_raw.c
tests_test_raw_LDADD = libn3.a ../b3/libb3.a


TESTS = $(check_PROGRAMS)
