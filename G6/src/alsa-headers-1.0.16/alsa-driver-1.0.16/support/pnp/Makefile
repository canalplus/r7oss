#
# Makefile for ALSA
# Copyright (c) 2001 by Jaroslav Kysela <perex@perex.cz>
#

TOPDIR = ../..

include $(TOPDIR)/toplevel.config
include $(TOPDIR)/Makefile.conf

TOPDIR = $(MAINSRCDIR)

export-objs  += pnp.o

snd-pnp-objs := pnp.o

# Toplevel Module Dependency
obj-m := snd-pnp.o

include $(TOPDIR)/Rules.make
