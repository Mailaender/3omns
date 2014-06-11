AC_PREREQ([2.68])
AC_INIT([3omns], [0.1], [chaz@chazomatic.us], [],
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


iconthemedir='${datarootdir}/icons/hicolor'

dnl TODO: this shouldn't be a --with arg, it should be just --iconthemedir.
AC_ARG_WITH([iconthemedir],
	[AS_HELP_STRING([--with-iconthemedir=ARG], [set icon theme directory
			(default: ${datarootdir}/icons/hicolor)])],
	[
		if test "$withval" = yes -o "$withval" = no; then
			AC_MSG_ERROR([you must specify the icon theme directory in --with-iconthemedir])
		else
			iconthemedir=$with_iconthemedir
		fi
	]
)

AC_SUBST(iconthemedir)


desktopdir='${datarootdir}/applications'

dnl TODO: this too should be just --desktopdir, not --with.
AC_ARG_WITH([desktopdir],
	[AS_HELP_STRING([--with-desktopdir=ARG], [set desktop directory
			(default: ${datarootdir}/applications)])],
	[
		if test "$withval" = yes -o "$withval" = no; then
			AC_MSG_ERROR([you must specify the desktop directory in --with-desktopdir])
		else
			desktopdir=$with_desktopdir
		fi
	]
)

AC_SUBST(desktopdir)


AC_ARG_ENABLE([manpages],
	[AS_HELP_STRING([--disable-manpages], [disable manpage generation])],
	[generate_manpages=$enableval]
)
AS_IF([test "$generate_manpages" != no], [
	AC_CHECK_PROG([A2X_BIN], [a2x], [a2x])
])

if test "$generate_manpages" = yes -a -z "$A2X_BIN"; then
	# They've requested manpage generation, but we can't find a2x.
	AC_MSG_ERROR([cannot find the AsciiDoc a2x binary needed to generate manpages])
elif test "$generate_manpages" != no; then
	generate_manpages=yes
	if test -z "$A2X_BIN"; then
		# They haven't specified and we can't find a2x.
		AC_MSG_WARN([disabling manpage generation because the AsciiDoc a2x binary is not in your PATH])
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

Compiler         : $CC $CPPFLAGS $CFLAGS
Generate manpages: $generate_manpages

Installation directories:
  prefix      : $prefix
  exec_prefix : $exec_prefix
  bindir      : $bindir
  datarootdir : $datarootdir
  datadir     : $datadir
  iconthemedir: $iconthemedir
  desktopdir  : $desktopdir
"
