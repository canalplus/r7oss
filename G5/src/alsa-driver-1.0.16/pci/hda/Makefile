ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := hda_codec.c

export-objs  := hda_codec.o

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/pci/hda

include $(SND_TOPDIR)/alsa-kernel/pci/hda/Makefile

include $(SND_TOPDIR)/Rules.make

hda_codec.c: hda_codec.patch $(SND_TOPDIR)/alsa-kernel/pci/hda/hda_codec.c
