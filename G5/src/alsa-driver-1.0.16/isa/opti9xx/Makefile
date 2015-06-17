ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

isapnp-files = opti92x-ad1848.isapnp opti92x-cs4231.isapnp opti93x.isapnp miro.isapnp

include $(SND_TOPDIR)/alsa-kernel/isa/opti9xx/Makefile

include $(SND_TOPDIR)/Rules.make
