ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := trident_main.o trident_memory.o

include $(SND_TOPDIR)/alsa-kernel/pci/trident/Makefile

include $(SND_TOPDIR)/Rules.make
