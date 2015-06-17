ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

snd-pcsp-objs := pcsp.o pcsp_lib.o pcsp_mixer.o pcsp_input.o
obj-$(CONFIG_SND_PCSP) += snd-pcsp.o

include $(SND_TOPDIR)/Rules.make
