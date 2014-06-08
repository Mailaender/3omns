AC_PREREQ([2.68])
AC_INIT([3omns], [0.0.0+git], [chaz@chazomatic.us], [],
		[http://chazomaticus.github.io/3omns/])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip])

AC_CONFIG_FILES([
	Makefile
	b3/Makefile
	l3/Makefile
	n3/Makefile
	res/Makefile
	src/Makefile
])

PKG_CHECK_MODULES([DEPS], [
	sdl2 >= 2.0.0
	SDL2_image >= 2.0.0
	SDL2_ttf >= 2.0.12
	lua5.2 >= 5.2.0
])
AC_CHECK_HEADERS([argp.h], [],
	[AC_MSG_ERROR([cannot find required header argp.h])]
)


appicondir='${datarootdir}/icons/hicolor/scalable/apps'

dnl TODO: this shouldn't be a --with arg, it should be just --appicondir.  How?
AC_ARG_WITH([appicondir],
	[AS_HELP_STRING([--with-appicondir=ARG], [set directory to install app
			icons (default:
			${datarootdir}/icons/hicolor/scalable/apps)])],
	[
		if test "$withval" = yes -o "$withval" = no; then
			AC_MSG_ERROR([you must specify the app icon directory in --with-appicondir])
		else
			appicondir=$with_appicondir
		fi
	]
)

AC_SUBST(appicondir)


AC_ARG_ENABLE([manpages],
	[AS_HELP_STRING([--disable-manpages], [disable manpage generation])],
	[generate_manpages=$enableval]
)

AC_ARG_WITH([a2x],
	[AS_HELP_STRING([--with-a2x=ARG], [specify the location of the a2x
			binary, used to generate manpages])],
	[
		if test "$withval" = yes -o "$withval" = no; then
			AC_MSG_ERROR([you must specify the location of a2x in --with-a2x])
		else
			A2X_BIN=$with_a2x
			dnl We could also test that $A2X_BIN is a valid binary,
			dnl but setting up the proper test seemed like a bit
			dnl much work.  Instead, we just trust the user.
		fi
	]
)
AS_IF([test "$generate_manpages" != no], [
	AC_CHECK_PROG([A2X_BIN], [a2x], [a2x])
])

no_a2x_msg=`echo "cannot find the a2x binary needed to generate manpages; try
installing AsciiDoc in a standard location or specifying the path to a2x in
--with-a2x" | tr '\n' ' '`
if test "$generate_manpages" = yes -a -z "$A2X_BIN"; then
	# They've requested manpage generation, but we can't find a2x.
	AC_MSG_ERROR([$no_a2x_msg])
elif test "$generate_manpages" != no; then
	generate_manpages=yes
	if test -z "$A2X_BIN"; then
		# They haven't specified and we can't find a2x.
		AC_MSG_NOTICE([$no_a2x_msg])
		AC_MSG_WARN([disabling manpage generation])
		generate_manpages=no
	fi
fi

AM_CONDITIONAL([GENERATE_MANPAGES], [test "$generate_manpages" != no])
AM_COND_IF([GENERATE_MANPAGES], [AC_CONFIG_FILES([3omns.6.txt])])


AC_PROG_CC_C99 dnl Actually, I want C11, but this seems to work.
AC_PROG_RANLIB

AC_OUTPUT


echo "
Successfully configured $PACKAGE_STRING

Compiler: $CC $CPPFLAGS $CFLAGS

Installation directories:
  prefix     : $prefix
  exec_prefix: $exec_prefix
  bindir     : $bindir
  datarootdir: $datarootdir
  datadir    : $datadir
  appicondir : $appicondir

Manpage generation options:
  Generate? : $generate_manpages
  a2x binary: $A2X_BIN
"
