if ENABLE_TOOLS
SUBDIRS = tools
else
SUBDIRS = src tools data doc.orig examples
endif

if TESTS
SUBDIRS += tests
endif

EXTRA_DIST = autogen.sh bootstrap libdbus-c++.spec libdbus-c++.spec.in

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dbus-c++-1.pc

MAINTAINERCLEANFILES = \
	configure \
	Makefile.in \
	aclocal.m4 \
	compile \
	config.guess \
	config.sub \
	depcomp \
	install-sh \
	ltmain.sh \
	mdate-sh \
	missing \
	mkinstalldirs \
	libdbus-c++.spec

