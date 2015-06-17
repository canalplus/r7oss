ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

# ALSA extra definitions

clean-files := mts64.c portman2x4.c

snd-serialmidi-objs := serialmidi.o
snd-aloop-objs := aloop.o

obj-$(CONFIG_SND) += pcsp/

obj-$(CONFIG_SND_SERIALMIDI) += snd-serialmidi.o
obj-$(CONFIG_SND_LOOPBACK) += snd-aloop.o

include $(SND_TOPDIR)/alsa-kernel/drivers/Makefile

include $(SND_TOPDIR)/Rules.make

mts64.c: mts64.patch $(SND_TOPDIR)/alsa-kernel/drivers/mts64.c
portman2x4.c: portman2x4.patch $(SND_TOPDIR)/alsa-kernel/drivers/portman2x4.c
