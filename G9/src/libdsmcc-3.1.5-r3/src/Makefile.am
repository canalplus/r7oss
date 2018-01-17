lib_LTLIBRARIES = libdsmcc.la

libdsmcc_la_LIBADD =

libdsmcc_la_SOURCES = \
	dsmcc-biop-ior.c \
	dsmcc-biop-tap.c \
	dsmcc-descriptor.c \
	dsmcc-ts.c \
	dsmcc-cache-module.c \
	dsmcc-debug.c \
	dsmcc.c \
	dsmcc-biop-message.c \
	dsmcc-biop-module.c \
	dsmcc-section.c \
	dsmcc-util.c \
	dsmcc-cache-file.c \
	dsmcc-carousel.c \
	dsmcc-gii.c

noinst_HEADERS = \
	dsmcc-biop-ior.h \
	dsmcc-biop-message.h \
	dsmcc-biop-module.h \
	dsmcc-biop-tap.h \
	dsmcc-cache.h \
	dsmcc-cache-file.h \
	dsmcc-cache-module.h \
	dsmcc-carousel.h \
	dsmcc-gii.h \
	dsmcc-config.h \
	dsmcc-compress.h \
	dsmcc-debug.h \
	dsmcc-descriptor.h \
	dsmcc.h \
	dsmcc-ts.h \
	dsmcc-util.h

AM_LDFLAGS = -lrt

if HAVE_ZLIB
libdsmcc_la_LIBADD += -lz
libdsmcc_la_SOURCES += \
	dsmcc-compress.c
endif

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src
