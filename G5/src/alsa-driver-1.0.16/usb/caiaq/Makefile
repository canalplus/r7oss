ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := caiaq-audio.c caiaq-device.c caiaq-input.c

include $(SND_TOPDIR)/alsa-kernel/usb/caiaq/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/usb/caiaq

include $(SND_TOPDIR)/Rules.make

caiaq-audio.c: caiaq-audio.patch $(SND_TOPDIR)/alsa-kernel/usb/caiaq/caiaq-audio.c
caiaq-device.c: caiaq-device.patch $(SND_TOPDIR)/alsa-kernel/usb/caiaq/caiaq-device.c
caiaq-input.c: caiaq-input.patch $(SND_TOPDIR)/alsa-kernel/usb/caiaq/caiaq-input.c
