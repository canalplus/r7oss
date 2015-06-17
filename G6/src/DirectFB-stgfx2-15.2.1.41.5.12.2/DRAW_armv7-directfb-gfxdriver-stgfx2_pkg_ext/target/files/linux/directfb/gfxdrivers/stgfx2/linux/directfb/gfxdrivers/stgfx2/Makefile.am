## Makefile.am for DirectFB/gfxdrivers/stgfx2

INCLUDES = \
	-I$(top_builddir)/linux/kernel/include \
	-I$(top_builddir)/linux/kernel/drivers/stm/blitter \
	-I$(top_srcdir)/linux/kernel/include \
	-I$(top_srcdir)/linux/kernel/drivers/stm/blitter


BDISP2_FEATURES = \
	-UBDISP2_DUMP_CHECK_STATE \
	-UBDISP2_DUMP_CHECK_STATE_FAILED \
	-UBDISP2_DUMP_SET_STATE

STGFX2_FEATURES = \
	-USTGFX2_DUMP_CHECK_STATE \
	-USTGFX2_DUMP_CHECK_STATE_FAILED \
	-USTGFX2_DUMP_SET_STATE

AM_CPPFLAGS = -DSTGFX_DRIVER=2 $(BDISP2_FEATURES) $(STGFX2_FEATURES)
AM_CFLAGS = $(DFB_CFLAGS) $(STGFX2_CFLAGS) -Wswitch-enum
AM_CXXFLAGS = $(AM_CFLAGS)

stgfx2_LTLIBRARIES = libdirectfb_stgfx2.la

if BUILD_STATIC
stgfx2_DATA = $(stgfx2_LTLIBRARIES:.la=.o)
endif

stgfx2dir = $(MODULEDIR)/gfxdrivers

if BUILD_STGFX2_ENGINE
engine_files = stgfx2_engine.cpp
else
engine_files =
endif

libdirectfb_stgfx2_la_SOURCES =	\
	stm_gfxdriver.c		\
	\
	$(engine_files)		\
	bdisp2_directfb_glue.c	\
	directfb_state_glue.c	\
	\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/strings.c			\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/bdisp2/bdispII_aq_features.c	\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/bdisp2/bdispII_aq_operations.c	\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/bdisp2/bdispII_aq_state.c	\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/bdisp2/bdispII_device_features.c	\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/bdisp2/bdispII_nodes.c		\
	\
	stgfx_screen.c		\
	stgfx_primary.c

if ENABLE_DEBUG
libdirectfb_stgfx2_la_SOURCES +=	\
	$(top_srcdir)/linux/kernel/drivers/stm/blitter/bdisp2/bdispII_debug.c
endif

libdirectfb_stgfx2_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	-export-symbols-regex bdisp_aq_StretchBlit_RLE

libdirectfb_stgfx2_la_LIBADD = \
	$(RT_LIBS) \
	$(DFB_LIBS)
