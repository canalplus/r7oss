ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs := opl4_lib.o

include $(SND_TOPDIR)/alsa-kernel/drivers/opl4/Makefile

include $(SND_TOPDIR)/Rules.make
