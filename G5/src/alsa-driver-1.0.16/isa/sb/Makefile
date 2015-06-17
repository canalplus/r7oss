ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

export-objs  := emu8000.o sb_common.o sb8_main.o sb16_main.o sb16_csp.o

isapnp-files := es968.isapnp sb16.isapnp sbawe.isapnp

clean-files := sb16_csp.c

include $(SND_TOPDIR)/alsa-kernel/isa/sb/Makefile

include $(SND_TOPDIR)/Rules.make

sb16_csp.c: sb16_csp.patch $(SND_TOPDIR)/alsa-kernel/isa/sb/sb16_csp.c
