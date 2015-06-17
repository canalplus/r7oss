bin_PROGRAMS =

if TOOLS
bin_PROGRAMS += dsmcc_reader
dsmcc_reader_SOURCES = dsmcc-reader.c
dsmcc_reader_CFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include
dsmcc_reader_LDADD = ../src/libdsmcc.la
endif
