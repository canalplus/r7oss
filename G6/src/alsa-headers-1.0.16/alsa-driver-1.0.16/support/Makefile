#
# Makefile for ALSA
# Copyright (c) 2001 by Jaroslav Kysela <perex@perex.cz>
#

TOPDIR = ..

include $(TOPDIR)/toplevel.config
include $(TOPDIR)/Makefile.conf

TOPDIR = $(MAINSRCDIR)

subdir-m :=

ifndef CONFIG_ISAPNP_KERNEL
ifeq (y,$(CONFIG_ISAPNP))
subdir-m += isapnp
endif
endif

ifndef CONFIG_PNP_KERNEL
ifeq (y,$(CONFIG_ISAPNP))
subdir-m += pnp
endif
endif

include $(TOPDIR)/Rules.make
