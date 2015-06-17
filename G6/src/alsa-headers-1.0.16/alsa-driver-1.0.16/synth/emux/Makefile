ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := emux.o emux_synth.o soundfont.o

include $(SND_TOPDIR)/alsa-kernel/synth/emux/Makefile

include $(SND_TOPDIR)/Rules.make
