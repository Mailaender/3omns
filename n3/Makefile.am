AM_CPPFLAGS = -Wall -I $(top_srcdir)

noinst_LIBRARIES = libn3.a
libn3_a_SOURCES = n3.h n3.c


TESTS = tests/core_send_receive
check_PROGRAMS = $(TESTS)

tests_core_send_receive_SOURCES = tests/test.h tests/core_send_receive.c
tests_core_send_receive_LDADD = libn3.a ../b3/libb3.a
