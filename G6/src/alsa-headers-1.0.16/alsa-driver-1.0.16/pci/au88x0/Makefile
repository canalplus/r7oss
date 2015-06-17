ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := au88x0.c

include $(SND_TOPDIR)/alsa-kernel/pci/au88x0/Makefile

include $(SND_TOPDIR)/Rules.make

au8810.c au8820.c au8830.c: au88x0.c

au88x0.c: au88x0.patch $(SND_TOPDIR)/alsa-kernel/pci/au88x0/au88x0.c
