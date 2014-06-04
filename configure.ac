AC_PREREQ([2.68])
AC_INIT([3omns], [0.0.0+git], [chaz@chazomatic.us])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip])

AC_CONFIG_FILES([
	Makefile
	b3/Makefile
	l3/Makefile
	n3/Makefile
	res/Makefile
	src/Makefile
])

AC_CHECK_HEADERS([argp.h], [],
	AC_MSG_ERROR([Can't find required header argp.h]) dnl '
)
PKG_CHECK_MODULES([DEPS], [
	sdl2 >= 2.0.0
	SDL2_image >= 2.0.0
	SDL2_ttf >= 2.0.12
	lua5.2 >= 5.2.0
])

AC_PROG_CC_C99 dnl Actually, I want C11, but this seems to work.
AC_PROG_RANLIB

AC_OUTPUT
