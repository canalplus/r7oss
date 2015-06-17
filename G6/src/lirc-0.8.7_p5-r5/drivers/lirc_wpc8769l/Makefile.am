## $Id: Makefile.am,v 1.1 2009/01/03 17:30:35 lirc Exp $

## Process this file with automake to produce Makefile.in

## this is so that Automake includes the C compiling definitions, and
## includes the source files in the distribution.
EXTRA_PROGRAMS = automake_dummy
automake_dummy_SOURCES = lirc_wpc8769l.c lirc_wpc8769l.h

## there is no *just* object file support in automake.  This is close enough
module_DATA = lirc_wpc8769l.o

include ../Makefile.common
