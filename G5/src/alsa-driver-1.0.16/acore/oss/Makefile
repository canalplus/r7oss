ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := mixer_oss.c pcm_oss.c

export-objs  := mixer_oss.o

include $(SND_TOPDIR)/alsa-kernel/core/oss/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/core/oss
# avoid oops due to the compiler bug (?) of old gcc
EXTRA_CFLAGS += -fno-omit-frame-pointer

include $(SND_TOPDIR)/Rules.make

mixer_oss.c: mixer_oss.patch $(SND_TOPDIR)/alsa-kernel/core/oss/mixer_oss.c
pcm_oss.c: pcm_oss.patch $(SND_TOPDIR)/alsa-kernel/core/oss/pcm_oss.c

