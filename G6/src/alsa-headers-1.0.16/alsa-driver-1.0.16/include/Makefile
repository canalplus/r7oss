#
# Makefile for ALSA driver
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@perex.cz>
#

include ../Makefile.conf

all: $(TARGETS)
	@echo "What's?" 

clean:
	rm -f .depend core *.o sndversions.h modules/*.ver modules/*.stamp

mrproper: clean
	rm -f *.orig *.rej *~ .#*
	rm -f linux/* asm/* media/*

