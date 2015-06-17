ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := ali5451.c

include $(SND_TOPDIR)/alsa-kernel/pci/ali5451/Makefile

include $(SND_TOPDIR)/Rules.make

ali5451.c: ali5451.patch $(SND_TOPDIR)/alsa-kernel/pci/ali5451/ali5451.c
