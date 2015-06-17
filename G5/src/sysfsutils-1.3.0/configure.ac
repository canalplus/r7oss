# Process this file with autoconf to produce a configure script.
AC_INIT(sysfsutils, 1.3.0, linux-diag-devel@lists.sourceforge.net)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([config.h.in])
AM_CONFIG_HEADER([config.h])
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_AWK

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h malloc.h mntent.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_LSTAT
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([bzero getmntent isascii memset strchr strerror strrchr strstr strtol])

AC_CONFIG_FILES([Makefile
                 cmd/Makefile
                 lib/Makefile
                 test/Makefile])
AC_OUTPUT
