
AC_INIT([toposet], [20100218], [laugeai@wyplay.com])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([src/toposet.c])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_RANLIB

dnl check alsa option and lib
alsa_CFLAGS=""
alsa_LIBS=""

AC_ARG_ENABLE(alsa, AS_HELP_STRING([--enable-alsa],
				   [enable alsa support]),
	      en_alsa=$enableval, en_alsa="no")

if test "$en_alsa" = "yes"; then
	AC_DEFINE([USE_ALSA], 1, [enable alsa support])

	PKG_CHECK_MODULES(alsa, alsa)

	pc_requires="$pc_requires alsa"
fi

CFLAGS="$CFLAGS $alsa_CFLAGS -Wall -Wno-unused -Wno-pointer-sign"


AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_HEADERS([include/config.h])
AC_OUTPUT

