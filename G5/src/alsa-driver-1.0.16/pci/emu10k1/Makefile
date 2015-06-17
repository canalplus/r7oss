ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := memory.o voice.o io.o

clean-files := emu10k1_main.c emu10k1x.c

include $(SND_TOPDIR)/alsa-kernel/pci/emu10k1/Makefile

include $(SND_TOPDIR)/Rules.make

emu10k1_main.c: emu10k1_main.patch $(SND_TOPDIR)/alsa-kernel/pci/emu10k1/emu10k1_main.c
emu10k1x.c: emu10k1x.patch $(SND_TOPDIR)/alsa-kernel/pci/emu10k1/emu10k1x.c
