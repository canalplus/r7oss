dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.52)

AC_INIT([stgfx2], [0.9], [http://bugzilla.stlinux.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([linux/directfb/gfxdrivers/stgfx2/Makefile.am])
AM_INIT_AUTOMAKE([foreign 1.5 no-define])

AM_MAINTAINER_MODE

AM_SANITY_CHECK

AC_CONFIG_HEADERS([config.h])
AC_HEADER_STDC

AC_ENABLE_SHARED
AC_DISABLE_STATIC

# check for programs
AC_PROG_CC
AC_PROG_CXX
AC_ISC_POSIX
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_C_BIGENDIAN

# compile tests using the C-compiler
AC_LANG([C])

AC_CHECK_HEADERS([stdbool.h])


if test "x$GCC" = xyes; then
  STGFX2_CFLAGS="-O3 -fexpensive-optimizations -ffast-math -Wall -Werror"
fi


#
# Find pkg-config
#
PKG_PROG_PKG_CONFIG
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test x$PKG_CONFIG = xno ; then
  AC_MSG_ERROR([*** pkg-config not found. See http://pkgconfig.sourceforge.net])
fi

if ! pkg-config --atleast-pkgconfig-version 0.9 ; then
  AC_MSG_ERROR([*** pkg-config too old; version 0.9 or better required.])
fi

#
# Check for DirectFB and DirectFB-Internal
#
DIRECTFB_REQUIRED_VERSION="1.4.12"

AC_MSG_CHECKING([for DirectFB >= $DIRECTFB_REQUIRED_VERSION])
if $PKG_CONFIG --atleast-version $DIRECTFB_REQUIRED_VERSION directfb ; then
  DFB_VERSION=`$PKG_CONFIG --modversion directfb`
  AC_MSG_RESULT([found (version $DFB_VERSION)])
  AC_MSG_CHECKING([for DirectFB-Internal = $DFB_VERSION])
  if $PKG_CONFIG --exact-version $DFB_VERSION directfb-internal ; then
    DFB_CFLAGS=`$PKG_CONFIG --cflags directfb directfb-internal`
    # We want to compile with -Wswitch-enum, but DirectFB headers cause
    # warnings with this enabled. By using -isystem instead of -I, gcc treats
    # them as 'system headers' for the purpose of warnings, and since we
    # don't enable -Wsystem-headers, we're safe.
    DFB_CFLAGS=`echo $DFB_CFLAGS | sed -e 's,-I/,-isystem /,g'`
    DFB_LIBS=`$PKG_CONFIG --libs directfb`
    MODULEDIR=$libdir/`$PKG_CONFIG --variable=moduledirname directfb-internal`
    DFB_INTERNAL_VERSION=`$PKG_CONFIG --modversion directfb`
    AC_MSG_RESULT([found (version $DFB_INTERNAL_VERSION)])
  else
    AC_MSG_ERROR([*** DirectFB-Internal is required and needs to match the version of DirectFB. ])
  fi

  orig_CFLAGS="$CFLAGS"
  orig_CXXFLAGS="$CXXFLAGS"
  CFLAGS="$CFLAGS $DFB_CFLAGS"
  CXXFLAGS="$CFLAGS $DFB_CFLAGS"

  # we test for DWOP_STEREO_SIDE_BY_SIDE_HALF because that was added after the intial support
  # for stereo
  AC_CHECK_DECLS([DWOP_STEREO_SIDE_BY_SIDE_HALF], [], [], [[#include <directfb.h>]])
  AC_MSG_CHECKING([for stereo API support in DirectFB])
  if test "x$ac_cv_have_decl_DWOP_STEREO_SIDE_BY_SIDE_HALF" = "xyes" ; then
    AC_DEFINE([HAVE_DFB_STEREO_API], [1], [Define to 1 if DirectFB has full stereo API support.])
  fi
  AC_MSG_RESULT([$ac_cv_have_decl_DWOP_STEREO_SIDE_BY_SIDE_HALF])

  AC_CHECK_DECLS([DSBLIT_FIXEDPOINT], [], [], [[#include <directfb.h>]])

  AC_CHECK_DECLS([DSBLIT_SOURCE2], [], [], [[#include <directfb.h>]])

  AC_CHECK_DECLS([CORE_DRMKMS], [], [], [[#include <core/system.h>]])

  AC_LANG_PUSH([C++])
  AC_CHECK_TYPES([DirectFB::Engine],
                 [AM_CONDITIONAL([BUILD_STGFX2_ENGINE], [true])],
                 [AM_CONDITIONAL([BUILD_STGFX2_ENGINE], [false])],
                 [[#include <core/Renderer.h>]])
  AC_LANG_POP([C++])

  formats=""
  AC_CHECK_DECLS([DSPF_ABGR],       [formats="$formats ABGR"],       [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_RGBAF88871], [formats="$formats RGBAF88871"], [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_LUT4],       [formats="$formats LUT4"],       [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_ALUT8],      [formats="$formats ALUT8"],      [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_LUT1], [formats="$formats LUT1"], [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_BGR24],      [formats="$formats BGR24"],      [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_NV12MB],     [formats="$formats NV12MB"],     [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_NV16MB],     [formats="$formats NV16MB"],     [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_BYTE],       [formats="$formats BYTE"],       [], [[#include <directfb.h>]])
  AC_CHECK_DECLS([DSPF_NV24],       [formats="$formats NV24"],       [], [[#include <directfb.h>]])

  AC_MSG_CHECKING([for conditional pixelformats in DirectFB])
  AC_MSG_RESULT([$formats])

  CXXFLAGS="$orig_CXXFLAGS"
  CFLAGS="$orig_CFLAGS"
else
  AC_MSG_ERROR([*** DirectFB $DIRECTFB_REQUIRED_VERSION or newer is required. ])
fi

#
# Conditional debugging support
#
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug],
                             [enable debugging @<:@default=no@:>@]),
              [], [enable_debug=no])
if test "x$enable_debug" = xyes; then
    STGFX2_CFLAGS="$STGFX2_CFLAGS -g3 -DDIRECT_FORCE_DEBUG -DDEBUG"
else
    STGFX2_CFLAGS="$STGFX2_CFLAGS -g0"
fi
AM_CONDITIONAL(ENABLE_DEBUG, test "x$enable_debug" = xyes)

AM_CONDITIONAL(BUILD_SHARED, test "$enable_shared" = "yes")
AM_CONDITIONAL(BUILD_STATIC, test "$enable_static" = "yes")

#
# Check librt presence
#
AC_CHECK_LIB(rt, clock_gettime, [RT_LIBS="-lrt"],)
AC_SUBST(RT_LIBS)


# Honor aclocal flags
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

AC_SUBST(STGFX2_CFLAGS)
AC_SUBST(DFB_CFLAGS)
AC_SUBST(DFB_LIBS)
AC_SUBST(MODULEDIR)
AC_SUBST(PACKAGE)

AC_OUTPUT([
Makefile
linux/directfb/gfxdrivers/stgfx2/Makefile
])

AC_MSG_RESULT([
Build options:
  Module directory       $MODULEDIR
  Debug mode             $enable_debug
])
