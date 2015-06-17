#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([dsmcc], [3.1.4], [dev@wyplay.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/dsmcc.c])
AC_CONFIG_HEADERS([src/dsmcc-config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [], [ AC_MSG_ERROR([Could not find pthread library]) ] )

# Checks for header files.
AC_CHECK_HEADERS([stdbool.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

######## zlib ########
AC_ARG_ENABLE(zlib, AC_HELP_STRING([--enable-zlib], [turn on zlib decompression support (default=enabled)]),
   [ en_zlib=$enableval ], [ en_zlib=yes ])

if test x$en_zlib == xyes ; then
   AC_CHECK_HEADER([zlib.h], [], [ AC_MSG_ERROR([Could not find zlib header file]) ])
   AC_CHECK_LIB([z], [deflate], [], [ AC_MSG_ERROR([Could not find zlib library]) ] )
   AM_CONDITIONAL([HAVE_ZLIB], true)
   AC_DEFINE([HAVE_ZLIB], 1, [Support for zlib compression])
   AC_SUBST([ZLIB_LIBS], "-lz")
else
   AM_CONDITIONAL([HAVE_ZLIB], false)
   AC_SUBST([ZLIB_LIBS], [])
fi

######## Debugging ########
AH_TEMPLATE([DEBUG], [Support for debugging.])
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [turn on debugging (default=disabled)]),
   [ en_debug=$enableval ], [ en_debug=no ])

if test x$en_debug == xyes ; then
   AM_CONDITIONAL([DEBUG], true)
   AC_DEFINE([DEBUG])
   CFLAGS="$CFLAGS -g -ggdb -W -Wall -Werror"
   CXXFLAGS=$CFLAGS
else
   AM_CONDITIONAL([DEBUG], false)
   CFLAGS="$CFLAGS -W -Wall -DNDEBUG"
   CXXFLAGS=$CFLAGS
fi

######## Tools ########
AC_ARG_ENABLE(tools, AC_HELP_STRING([--enable-tools], [turn on tools (default=enabled)]),
   [ en_tools=$enableval ], [ en_tools=yes ])
AM_CONDITIONAL([TOOLS], test x$en_tools == xyes)

# REQUIRED_PKG_CONFIG can be used inside dsmcc.pc.in file
AC_SUBST([REQUIRED_PKG_CONFIG], ${REQUIRED_PKG_CONFIG})
AC_CONFIG_FILES([ Makefile Doxyfile include/Makefile src/Makefile tools/Makefile dsmcc.pc ])

######## Summary of configured options  ########
AC_MSG_NOTICE([***** Summary *****])
AC_MSG_NOTICE([DSMCC DEBUG          : $en_debug])
AC_MSG_NOTICE([DSMCC ZLIB           : $en_zlib])
AC_MSG_NOTICE([DSMCC TOOLS          : $en_tools])

AC_OUTPUT

