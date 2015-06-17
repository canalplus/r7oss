ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := ad1848_lib.o

include $(SND_TOPDIR)/alsa-kernel/isa/ad1848/Makefile

include $(SND_TOPDIR)/Rules.make
