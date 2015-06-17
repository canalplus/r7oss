## Makefile.am for DirectFB/gfxdrivers/stgfx2

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib   \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib     \
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

AM_CPPFLAGS = -DSTGFX_DRIVER=2

internalincludedir = $(INTERNALINCLUDEDIR)/stgfx2
internalinclude_HEADERS = \
	bdisp_accel.h

stgfx2_LTLIBRARIES = libdirectfb_stgfx2.la

if BUILD_STATIC
stgfx2_DATA = $(stgfx2_LTLIBRARIES:.la=.o)
endif

stgfx2dir = $(MODULEDIR)/gfxdrivers

libdirectfb_stgfx2_la_SOURCES =	\
	stgfx2_features.h	\
	stm_types.h		\
	\
	stm_gfxdriver.c		\
	stm_gfxdriver.h		\
	\
	bdisp_registers.h	\
	bdisp_tables.h		\
	bdisp_features.h	\
	bdisp_features.c	\
	bdisp_state.c		\
	bdisp_state.h		\
	bdisp_accel.c		\
	bdisp_accel.h		\
	bdisp_accel_types.h	\
	\
	stmfbdev_screen.c	\
	stmfbdev_layer.c	\
	\
	surfacemanager.c	\
	surfacemanager.h	\
	stmfbdev_surface_pool.c	\
	bdisp_surface_pool.c	\
	bdisp_surface_pool.h	\
	\
	stgfx_screen.c		\
	stgfx_screen.h		\
	stgfx_primary.c		\
	stgfx_primary.h

libdirectfb_stgfx2_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	-export-symbols-regex bdisp_aq_StretchBlit_RLE	\
	$(DFB_LDFLAGS)

libdirectfb_stgfx2_la_LIBADD = \
	-lrt \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
