ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs = ak4xxx.o

include $(SND_TOPDIR)/alsa-kernel/pci/ice1712/Makefile

include $(SND_TOPDIR)/Rules.make
