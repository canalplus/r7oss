ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := cs4231_lib.o cs4236_lib.o

isapnp-files := cs4232.isapnp cs4236.isapnp 

include $(SND_TOPDIR)/alsa-kernel/isa/cs423x/Makefile

snd-pc98-cs4232-objs := pc98.o
obj-$(CONFIG_SND_PC98_CS4232) += snd-pc98-cs4232.o snd-cs4231-lib.o

include $(SND_TOPDIR)/Rules.make
