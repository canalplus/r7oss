ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

snd-ioctl32-objs := ioctl32.o pcm32.o rawmidi32.o timer32.o hwdep32.o
ifneq ($(CONFIG_SND_SEQUENCER),n)
  snd-ioctl32-objs += seq32.o
endif

obj-$(CONFIG_SND_BIT32_EMUL) += snd-ioctl32.o

include $(SND_TOPDIR)/Rules.make

