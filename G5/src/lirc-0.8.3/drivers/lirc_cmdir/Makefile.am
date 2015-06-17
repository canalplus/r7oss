## $Id: Makefile.am,v 1.3 2007/07/02 22:29:53 innovationone Exp $

## Process this file with automake to produce Makefile.in

## this is so that Automake includes the C compiling definitions, and
## includes the source files in the distribution.
EXTRA_PROGRAMS = automake_dummy
automake_dummy_SOURCES = lirc_cmdir.c lirc_cmdir.h commandir.c commandir.h

## there is no *just* object file support in automake.  This is close enough
module_DATA = lirc_cmdir.o commandir.o

include ../Makefile.common
