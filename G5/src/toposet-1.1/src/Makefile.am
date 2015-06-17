

INCLUDES = -I$(top_srcdir)/include 
AM_CFLAGS = -fPIC -I$(top_srcdir)/include
instdir=$(pkglibdir)

inst_LIBRARIES = libtoposet.a

libtoposet_a_LDFLAGS = -lasound 
libtoposet_a_CFLAGS  = -I$(top_srcdir)/include -D main=toposet_main
libtoposet_a_SOURCES = toposet.c

bin_PROGRAMS = toposet

toposet_LDFLAGS= -lasound
toposet_LDADD= libtoposet.a
toposet_SOURCES = toposet_cmd.c 

