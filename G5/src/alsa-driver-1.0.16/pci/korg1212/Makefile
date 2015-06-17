ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := korg1212.c

include $(SND_TOPDIR)/alsa-kernel/pci/korg1212/Makefile

include $(SND_TOPDIR)/Rules.make

korg1212.c: korg1212.patch $(SND_TOPDIR)/alsa-kernel/pci/korg1212/korg1212.c
