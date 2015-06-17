#
# Makefile for ALSA driver
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@perex.cz>
#

TOPDIR  = ..

ifeq ($(TOPDIR)/Makefile.conf,$(wildcard $(TOPDIR)/Makefile.conf))
include $(TOPDIR)/Makefile.conf
else
CC = gcc
INCLUDE = -I../include
endif

TARGETS = dummy

.c.o:
	$(CC) $(INCLUDE) -Wall -g -c -o $*.o $<

all: $(TARGETS)

.PHONY: dummy
dummy:

mod-deps: mod-deps.o
	$(CC) -g $(INCLUDE) mod-deps.o -o $@

kredirect: kredirect.o
	$(CC) $(INCLUDE) kredirect.o -o $@

clean:
	rm -f core .depend *.o kredirect mod-deps

mrproper:
	rm -f *.orig *.rej *~ .#*

.depend:
	$(CPP) $(INCLUDE) *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
