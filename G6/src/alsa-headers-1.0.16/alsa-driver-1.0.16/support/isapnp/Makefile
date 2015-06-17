#
# Makefile for ALSA
# Copyright (c) 2001 by Jaroslav Kysela <perex@perex.cz>
#

TOPDIR = ../..

include $(TOPDIR)/toplevel.config
include $(TOPDIR)/Makefile.conf

TOPDIR = $(MAINSRCDIR)

export-objs     += isapnp.o

snd-isapnp-objs := isapnp.o isapnp_proc.o isapnp_quirks.o

# Toplevel Module Dependency
obj-m := snd-isapnp.o

include $(TOPDIR)/Rules.make
