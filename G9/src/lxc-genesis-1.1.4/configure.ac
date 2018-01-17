AC_INIT([lxc_genesis], [1.1.4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LEX
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_YACC

PKG_CHECK_MODULES([LIBCAP], [libcap >= 2.24])

# Enable warnings.
CFLAGS+=" -W -Wall -Wextra -Wno-unused-parameter"

#
# --enable-hardened
#
AC_ARG_ENABLE(
    [hardened],
        [AS_HELP_STRING(
            [--enable-hardened],
            [enabled hardened mode [default=no]])],
    [],
    [enable_hardened=no])

if test x$enable_hardened = xyes; then
    AC_DEFINE([ENABLE_HARDENED], [1], [hardened mode enabled])
fi

AM_CONDITIONAL([ENABLE_HARDENED], [test x$enable_hardened = xyes])

#
# --enable-debug
#
AC_ARG_ENABLE(
    [debug],
    [AS_HELP_STRING([--enable-debug], [turns on debug [default=no]])],
    [],
    [enable_debug=no])

if test x$enable_debug = xyes; then
    CFLAGS+=" -ggdb -O0"
    AC_DEFINE([DEBUG], [1], [debug enabled])
else
    CFLAGS+=" -DNDEBUG"
fi

AM_CONDITIONAL([ENABLE_DEBUG], [test x$enable_debug = xyes])

#
# --enable-config-debug
#
AC_ARG_ENABLE(
    [config-debug],
    [AS_HELP_STRING(
        [--enable-config-debug],
        [turns on configuration parser debugging [default=no]])],
    [],
    [enable_config_debug=no])

if test x$enable_config_debug = xyes; then
    AC_DEFINE([CONFIG_DEBUG], [1], [configuration parser debugging enabled])
fi

AM_CONDITIONAL([ENABLE_CONFIG_DEBUG], [test x$enable_config_debug = xyes])

#
# --enable-tests
#
AC_ARG_ENABLE(
    [tests],
    [AS_HELP_STRING(
        [--enable-tests],
        [build tests [default=no]])],
    [],
    [enable_tests=no])

if test x$enable_tests = xyes; then
    AC_CHECK_LIB(
        [cunit],
        [CU_initialize_registry],
        [:],
        [AC_MSG_ERROR([can't find cunit library])])
    AC_CHECK_LIB(
        [ncurses],
        [initscr],
        [AC_DEFINE([HAVE_LIBNCURSES], [1], [ncurses library available])
        ncurses_libs=-lncurses],
        [AC_MSG_NOTICE([cannot find ncurses library])
        ncurses_libs=])
    AC_SUBST([cunit_FLAGS], [])
    AC_SUBST([cunit_LIBS], ["-lcunit ${ncurses_libs}"])
fi

AM_CONDITIONAL([ENABLE_TESTS], [test x$enable_tests = xyes])

AC_CONFIG_FILES([
    Doxyfile
    Makefile
    lxc_genesis.pc
    lxc_genesis_core.pc
    src/Makefile
    tests/Makefile])

AC_OUTPUT
