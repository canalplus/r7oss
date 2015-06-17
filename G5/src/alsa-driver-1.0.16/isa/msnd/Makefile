ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

snd-msnd-pinnacle-objs := msnd.o msnd_pinnacle.o msnd_pinnacle_mixer.o msnd_midi.o
# snd-msnd-classic-objs := msnd.o msnd_classic.o msnd_midi.o

# Toplevel Module Dependency
obj-$(CONFIG_SND_MSND_PINNACLE) += snd-msnd-pinnacle.o
# obj-$(CONFIG_SND_MSND_CLASSIC) += snd-msnd-classic.o

include $(SND_TOPDIR)/Rules.make
