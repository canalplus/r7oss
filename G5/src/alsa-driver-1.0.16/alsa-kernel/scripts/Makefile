#
# Makefile for ALSA driver
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@perex.cz>
#

TARGETS = docproc

.c.o:
	$(CC) -g $(INCLUDE) -Wall -c -o $*.o $<

all: $(TARGETS)

docproc: docproc.o
	$(CC) $(INCLUDE) $< -o $@ 

clean:
	rm -f $(TARGETS)
	rm -f core .depend *.o *.orig *~ 

.depend:
	$(CPP) $(INCLUDE) *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
