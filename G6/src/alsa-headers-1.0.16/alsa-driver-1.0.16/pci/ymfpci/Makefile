ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := ymfpci_main.c

include $(SND_TOPDIR)/alsa-kernel/pci/ymfpci/Makefile

include $(SND_TOPDIR)/Rules.make

ymfpci_main.c: ymfpci_main.patch $(SND_TOPDIR)/alsa-kernel/pci/ymfpci/ymfpci_main.c
