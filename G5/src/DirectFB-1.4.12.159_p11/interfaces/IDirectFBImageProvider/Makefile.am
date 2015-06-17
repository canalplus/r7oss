## Makefile.am for DirectFB/interfaces/IDirectFBImageProvider

idirectfbimageproviderdir = $(MODULEDIR)/interfaces/IDirectFBImageProvider

if GIF_PROVIDER
GIF_PROVIDER_LTLIB = libidirectfbimageprovider_gif.la
else
GIF_PROVIDER_LTLIB = 
endif

if JPEG_PROVIDER
JPEG_PROVIDER_LTLIB = libidirectfbimageprovider_jpeg.la
else
JPEG_PROVIDER_LTLIB = 
endif

if PNG_PROVIDER
PNG_PROVIDER_LTLIB = libidirectfbimageprovider_png.la
else
PNG_PROVIDER_LTLIB = 
endif

if RLE_PROVIDER
RLE_PROVIDER_LTLIB = libidirectfbimageprovider_rle.la
else
RLE_PROVIDER_LTLIB =
endif


if MME_USE_OWN_HEADERS
MME_INCLUDE_PATH = -I$(top_srcdir)/interfaces/IDirectFBImageProvider/mme
else
MME_INCLUDE_PATH =
endif
if MME_USE_OWN_TRANSFORMERS
TRANSFORMER_INCLUDE_PATH = -I$(top_srcdir)/interfaces/IDirectFBImageProvider/mme/transform
else
TRANSFORMER_INCLUDE_PATH =
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems	\
	$(MME_INCLUDE_PATH)	\
	$(TRANSFORMER_INCLUDE_PATH)

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\" $(LIBJPEG_CFLAGS) $(LIBPNG_CFLAGS)

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_dfiff.la	\
	$(GIF_PROVIDER_LTLIB)			\
	$(JPEG_PROVIDER_LTLIB)			\
	$(PNG_PROVIDER_LTLIB)			\
	$(RLE_PROVIDER_LTLIB)

if BUILD_STATIC
idirectfbimageprovider_DATA = $(idirectfbimageprovider_LTLIBRARIES:.la=.o)
endif


libidirectfbimageprovider_png_la_SOURCES = idirectfbimageprovider_png.c

libidirectfbimageprovider_png_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBPNG_LIBS) \
	$(PNG_LIBMME)

libidirectfbimageprovider_png_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_dfiff_la_SOURCES = idirectfbimageprovider_dfiff.c

libidirectfbimageprovider_dfiff_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbimageprovider_dfiff_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_gif_la_SOURCES = idirectfbimageprovider_gif.c

libidirectfbimageprovider_gif_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbimageprovider_gif_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_jpeg_la_SOURCES = idirectfbimageprovider_jpeg.c

libidirectfbimageprovider_jpeg_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBJPEG) \
	$(JPEG_LIBMME)

libidirectfbimageprovider_jpeg_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_rle_la_SOURCES = idirectfbimageprovider_rle.c

libidirectfbimageprovider_rle_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(RLE_LIBMME)

libidirectfbimageprovider_rle_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make
