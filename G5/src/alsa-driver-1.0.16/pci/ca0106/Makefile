ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := ca0106_main.c

include $(SND_TOPDIR)/alsa-kernel/pci/ca0106/Makefile

include $(SND_TOPDIR)/Rules.make

ca0106_main.c: ca0106_main.patch $(SND_TOPDIR)/alsa-kernel/pci/ca0106/ca0106_main.c
