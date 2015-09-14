AM_CPPFLAGS = -Wall -I $(top_srcdir)

noinst_LIBRARIES = libn3.a
libn3_a_SOURCES = buffer.c internal.h n3.c n3.h ordered_list.h proto.c raw.c


TESTS = tests/test_raw


check_PROGRAMS = tests/n3c $(TESTS)

COMMON_LIBS = libn3.a ../b3/libb3.a

tests_n3c_SOURCES = tests/n3c.c
tests_n3c_LDADD = $(COMMON_LIBS)

tests_test_raw_SOURCES = tests/test.h tests/test_raw.c
tests_test_raw_LDADD = $(COMMON_LIBS)
