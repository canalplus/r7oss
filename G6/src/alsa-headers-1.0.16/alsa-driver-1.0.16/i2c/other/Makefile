ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs += ak4xxx-adda.o ak4114.o ak4117.o pt2258.o

# Toplevel Module Dependency
# hack for tea575x support
ifeq (y,$(CONFIG_VIDEO_DEV))
obj-$(CONFIG_SND_FM801) += snd-tea575x-tuner.o
ifeq (m,$(CONFIG_SND_FM801))
export-objs += tea575x-tuner.o
clean-files += tea575x-tuner.c
endif
endif

include $(SND_TOPDIR)/alsa-kernel/i2c/other/Makefile

include $(SND_TOPDIR)/Rules.make
