ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

extra-subdir-y  := other
extra-subdir-m  := $(extra-subdir-y)

export-objs  := i2c.o cs8427.o tea6330t.o

snd-uda1380-objs := uda1380.o
obj-$(CONFIG_SND_UDA1380) += snd-uda1380.o

include $(SND_TOPDIR)/alsa-kernel/i2c/Makefile

include $(SND_TOPDIR)/Rules.make
