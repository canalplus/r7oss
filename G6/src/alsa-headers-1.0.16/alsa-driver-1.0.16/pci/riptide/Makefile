ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := riptide.c

include $(SND_TOPDIR)/alsa-kernel/pci/riptide/Makefile

include $(SND_TOPDIR)/Rules.make

riptide.c: riptide.patch $(SND_TOPDIR)/alsa-kernel/pci/riptide/riptide.c
