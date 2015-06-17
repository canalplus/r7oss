## Makefile.am for DirectFB/gfxdrivers/stgfx

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib   \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib     \
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems	\
	-I$(top_srcdir)/gfxdrivers

AM_CPPFLAGS = -DSTGFX_DRIVER=1

stgfx_LTLIBRARIES = libdirectfb_stgfx.la

if BUILD_STATIC
stgfx_DATA = $(stgfx_LTLIBRARIES:.la=.o)
endif

stgfxdir = $(MODULEDIR)/gfxdrivers

libdirectfb_stgfx_la_SOURCES =	\
	stgfx.c				\
	stgfx.h				\
	\
	../stgfx2/stmfbdev_screen.c	\
	../stgfx2/stmfbdev_layer.c	\
	\
	../stgfx2/surfacemanager.c	\
	../stgfx2/surfacemanager.h	\
	../stgfx2/stmfbdev_surface_pool.c	\
	../stgfx2/bdisp_surface_pool.c	\
	../stgfx2/bdisp_surface_pool.h	\
	\
	../stgfx2/stgfx_screen.c	\
	../stgfx2/stgfx_screen.h	\
	../stgfx2/stgfx_primary.c	\
	../stgfx2/stgfx_primary.h

libdirectfb_stgfx_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_stgfx_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
