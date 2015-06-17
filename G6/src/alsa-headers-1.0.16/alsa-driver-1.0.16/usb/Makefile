ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := usbaudio.c usbmidi.c usbmixer.c

# for compatibility
extra-snd-usb-lib-objs := usbcompat.o
export-objs := usbmidi.o usbcompat.o

include $(SND_TOPDIR)/alsa-kernel/usb/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/usb

include $(SND_TOPDIR)/Rules.make

usbaudio.c: usbaudio.patch usbaudio.inc usbaudio.inc1 $(SND_TOPDIR)/alsa-kernel/usb/usbaudio.c
usbmidi.c: usbmidi.patch usbmidi.inc usbmidi.inc1 $(SND_TOPDIR)/alsa-kernel/usb/usbmidi.c
usbmixer.c: usbmixer.patch $(SND_TOPDIR)/alsa-kernel/usb/usbmixer.c
