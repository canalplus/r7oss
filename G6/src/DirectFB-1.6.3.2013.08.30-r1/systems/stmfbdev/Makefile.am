## Makefile.am for DirectFB/systems/stmfbdev

if STMFBDEV_USE_OWN_HEADERS
STMFB_INCLUDE_PATH = -I$(srcdir)/include
else
STMFB_INCLUDE_PATH =
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(STMFB_INCLUDE_PATH)


internalincludedir = $(INTERNALINCLUDEDIR)/stmfbdev

internalinclude_HEADERS = \
	fb.h			\
	vt.h			\
	surfacemanager.h	\
	stmfbdev.h

systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_stmfbdev.o
endif
systems_LTLIBRARIES = libdirectfb_stmfbdev.la

libdirectfb_stmfbdev_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_stmfbdev_la_SOURCES = \
	vt.c			\
	\
	stmfbdev_screen.c	\
	stmfbdev_layer.c	\
	\
	surfacemanager.c	\
	stmfbdev_surface_pool.c	\
	stmfb_aux_surface_pool.c	\
	\
	stmfbdev.c

libdirectfb_stmfbdev_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
