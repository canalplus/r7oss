ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := es1688_lib.o

include $(SND_TOPDIR)/alsa-kernel/isa/es1688/Makefile

include $(SND_TOPDIR)/Rules.make
