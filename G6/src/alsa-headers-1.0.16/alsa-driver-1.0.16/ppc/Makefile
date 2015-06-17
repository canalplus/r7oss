ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := awacs.c beep.c tumbler.c

include $(SND_TOPDIR)/alsa-kernel/ppc/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/ppc

include $(SND_TOPDIR)/Rules.make

awacs.c: awacs.patch $(SND_TOPDIR)/alsa-kernel/ppc/awacs.c
beep.c: beep.patch $(SND_TOPDIR)/alsa-kernel/ppc/beep.c
tumbler.c: tumbler.patch $(SND_TOPDIR)/alsa-kernel/ppc/tumbler.c
