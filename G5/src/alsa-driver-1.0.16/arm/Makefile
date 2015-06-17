ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := sa11xx-uda1341.c

include $(SND_TOPDIR)/alsa-kernel/arm/Makefile

snd-sa11x0-pcm-objs		:= sa11x0-pcm.o devdma.o
snd-pxa2xx-i2sound-objs		:= pxa2xx-i2sound.o

obj-$(CONFIG_SND_S3C2410)	+= s3c24xx-iis.o
obj-$(CONFIG_SND_PXA2xx_I2SOUND)	+= snd-pxa2xx-i2sound.o

include $(SND_TOPDIR)/Rules.make

sa11xx-uda1341.c: sa11xx-uda1341.patch $(SND_TOPDIR)/alsa-kernel/arm/sa11xx-uda1341.c
