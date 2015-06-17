ifndef SND_TOPDIR
SND_TOPDIR=../../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := seq_oss.c

include $(SND_TOPDIR)/alsa-kernel/core/seq/oss/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/core/seq/oss

include $(SND_TOPDIR)/Rules.make

seq_oss.c: seq_oss.patch $(SND_TOPDIR)/alsa-kernel/core/seq/oss/seq_oss.c
