ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := util_mem.o

include $(SND_TOPDIR)/alsa-kernel/synth/Makefile

include $(SND_TOPDIR)/Rules.make
