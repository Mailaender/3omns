AC_PREREQ([2.68])
AC_INIT([3omns], [0.0.0+git])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_FILES([
	Makefile
	b3/Makefile
	l3/Makefile
	src/Makefile
])

PKG_CHECK_MODULES([DEPS], [
	sdl2 >= 2.0.0
	SDL2_image >= 2.0.0
	lua5.2 >= 5.2.0
])

AC_PROG_CC_C99 dnl Actually, I want C11, but this seems to work.
AC_PROG_RANLIB

AC_OUTPUT
