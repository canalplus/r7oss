ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := gus_main.o gus_volume.o

isapnp-files := interwave.isapnp interwave-stb.isapnp

include $(SND_TOPDIR)/alsa-kernel/isa/gus/Makefile

include $(SND_TOPDIR)/Rules.make
