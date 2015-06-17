ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := virtuoso.c

include $(SND_TOPDIR)/alsa-kernel/pci/oxygen/Makefile

export-objs := oxygen_io.o oxygen_lib.o

include $(SND_TOPDIR)/Rules.make

virtuoso.c: virtuoso.patch $(SND_TOPDIR)/alsa-kernel/pci/oxygen/virtuoso.c
