AC_PREREQ([2.68])
AC_INIT([3omns], [0.0.0+git])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_FILES([
	Makefile
	b3/Makefile
	src/Makefile
])

AC_CHECK_PROG([sdl2_config], [sdl2-config], [sdl2-config])
if test "x${sdl2_config}" = x; then
	AC_MSG_ERROR(Missing required package SDL2)
fi
sdl2_cppflags=`"${sdl2_config}" --cflags`
sdl2_ldadd=`"${sdl2_config}" --libs`
AC_SUBST(sdl2_cppflags)
AC_SUBST(sdl2_ldadd)

AC_CHECK_HEADERS([SDL2/SDL_image.h], , [AC_MSG_ERROR(Missing required package SDL2_image)])
AC_SEARCH_LIBS([IMG_Init], [SDL2_image], , [AC_MSG_ERROR(Missing required package SDL2_image)])

AC_PROG_CC_C99 dnl Actually, I want C11, but this seems to work.
AC_PROG_RANLIB

AC_OUTPUT
