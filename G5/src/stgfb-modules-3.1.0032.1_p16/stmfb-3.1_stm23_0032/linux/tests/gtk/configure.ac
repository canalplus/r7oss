# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)

m4_define([v4l2_control_version], [0.5.0])

# required versions of other packages
m4_define([gtk_required_version], [2.12])

AC_INIT([v4l2-control], [v4l2_control_version],
        [http://www.stlinux.com],
        [v4l2-control])


AC_CONFIG_SRCDIR([src/v4l2-control-gtk.c])

AM_INIT_AUTOMAKE([no-define -Wno-portability dist-bzip2])
AM_CONFIG_HEADER(config.h)


# Support silent build rules, requires at least automake-1.11. Enable
# by either passing --enable-silent-rules to configure or passing V=0
# to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([no])])

AM_PROG_LIBTOOL
dnl when using libtool 2.x create libtool early, because it's used in configure
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])

# Make sure we use 64-bit versions of various file stuff.
AC_SYS_LARGEFILE

# Build time sanity check...
AM_SANITY_CHECK

# Checks for programs.
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Honor aclocal flags
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

## things we need - GTK+
PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= gtk_required_version])

## and V4L2 headers
AC_CHECK_HEADERS(linux/videodev.h)

AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_OUTPUT
