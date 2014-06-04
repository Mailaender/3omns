bin_PROGRAMS = ../3omns
___3omns_SOURCES = 3omns.h args.c main.c net.c
___3omns_CPPFLAGS = \
	-Wall \
	-I $(top_srcdir) \
	$(DEPS_CFLAGS) \
	-DDATADIR='"$(pkgdatadir)"'
___3omns_LDADD = ../n3/libn3.a ../l3/libl3.a ../b3/libb3.a $(DEPS_LIBS)
