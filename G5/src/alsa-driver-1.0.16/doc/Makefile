#
# Makefile for ALSA driver
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@perex.cz>
#

include ../Makefile.conf

SUBDIRS  = DocBook

all: $(TARGETS)
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d; then exit 1; fi; done

clean:
	rm -f core .depend *.orig *~
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d clean; then exit 1; fi; done
