ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

isapnp-files = wavefront.isapnp

clean-files := wavefront_fx.c wavefront_synth.c

SND_TOPDIR = $(MAINSRCDIR)
include $(SND_TOPDIR)/alsa-kernel/isa/wavefront/Makefile

include $(SND_TOPDIR)/Rules.make

wavefront_fx.c: wavefront_fx.patch $(SND_TOPDIR)/alsa-kernel/isa/wavefront/wavefront_fx.c
wavefront_synth.c: wavefront_synth.patch $(SND_TOPDIR)/alsa-kernel/isa/wavefront/wavefront_synth.c
