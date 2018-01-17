
AC_INIT([wytchmain], [1.1.7], [ccogeanu@wyplay.com])
AC_PREREQ([2.59])

AC_CONFIG_SRCDIR([src/tdaemon.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AM_PROG_CC_C_O

PKG_CHECK_MODULES(wylog, wylog >= 1.0)
PKG_CHECK_MODULES(wydbus, wydbus >= 1.5.0)
PKG_CHECK_MODULES(dbusxx, dbus-c++-1)
PKG_CHECK_MODULES(wycrs, wycrs >= 1.6.3)


PKG_CHECK_MODULES(wytools2, wytools-2 >= 2.0.6)

PKG_CHECK_MODULES(glib2, glib-2.0 >= 2.28.8)
PKG_CHECK_MODULES(gio2, gio-2.0 >= 2.28.8)


AC_CHECK_PROG(DBUSXX_XML2CPP, dbusxx-xml2cpp, dbusxx-xml2cpp)
[[ -z $DBUSXX_XML2CPP ]] && AC_ERROR(dbusxx-xml2cpp not found)

AC_SUBST(wycrs_DBUSXMLPATH, ${ROOT}/usr/share/wycrs)

AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h])

SUBPROJECTS="test xml"

AC_SUBST(subprojects, $SUBPROJECTS)

######## PID file ########
AC_ARG_WITH(pid_file_path,
              AC_HELP_STRING([--with-pid-file-path=FILE],
                             [set pid file (default=/var/run/wytchmain.pid)]),
              [pid_file_path="$withval"],
              [pid_file_path=/var/run/wytchmain.pid])

AH_TEMPLATE([WYTCHMAIN_PID_FILE], [PID file path.])
AC_DEFINE_UNQUOTED([WYTCHMAIN_PID_FILE], "$pid_file_path")

AH_TEMPLATE([WYTCHMAIN_USE_PID_FILE], [Use pid file.])
AC_ARG_ENABLE(pid_file,
              AC_HELP_STRING([--enable-pid-file=FILE],
                             [set pid file (default=no)]),
              [if test x$enable_pid_file = xyes; then 
                  AC_DEFINE([WYTCHMAIN_USE_PID_FILE],[1])
              else
                  AC_DEFINE([WYTCHMAIN_USE_PID_FILE],[0])
              fi],
              [AC_DEFINE([WYTCHMAIN_USE_PID_FILE],[0])])

######## DEBUG options #######

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [turns on debug compiler, default:no]))
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [1], [debug enabled])
dnl     AC_DEFINE([WYLOG_MODULE], [wytchmain], [Wylog module for WyTCHMain])
    CFLAGS="$CFLAGS -ggdb -W -Wall -O0"
else
    CFLAGS="$CFLAGS -W -Wall -DNDEBUG"
fi

######## SYSLOG options #######
AC_ARG_ENABLE(syslog, AS_HELP_STRING([--enable-syslog], [turns on syslog, default:no]))
if test x$enable_syslog = xyes; then
    AC_DEFINE([SYSLOG], [1], [syslog enabled])
dnl     AC_DEFINE([WYLOG_MODULE], [wytchmain], [Wylog module for WyTCHMain])
fi


AC_CONFIG_FILES([Makefile
                    include/Makefile
                    src/Makefile
                    test/Makefile
                    wytchmain.pc
                    xml/Makefile])
AC_OUTPUT

