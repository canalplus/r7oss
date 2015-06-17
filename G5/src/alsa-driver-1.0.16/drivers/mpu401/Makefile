ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := mpu401.c

export-objs  := mpu401_uart.o

include $(SND_TOPDIR)/alsa-kernel/drivers/mpu401/Makefile

include $(SND_TOPDIR)/Rules.make

mpu401.c: mpu401.patch $(SND_TOPDIR)/alsa-kernel/drivers/mpu401/mpu401.c
