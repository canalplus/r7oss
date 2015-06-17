ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

#
# ALSA extra code
#

clean-files := ad1889.c atiixp.c atiixp_modem.c bt87x.c cmipci.c ens1370.c \
	fm801.c intel8x0.c maestro3.c via82xx.c via82xx_modem.c

obj-$(CONFIG_SND) += pdplus/ asihpi/

#
#
#

include $(SND_TOPDIR)/alsa-kernel/pci/Makefile

include $(SND_TOPDIR)/Rules.make

ad1889.c: ad1889.patch $(SND_TOPDIR)/alsa-kernel/pci/ad1889.c
atiixp.c: atiixp.patch $(SND_TOPDIR)/alsa-kernel/pci/atiixp.c
atiixp_modem.c: atiixp_modem.patch $(SND_TOPDIR)/alsa-kernel/pci/atiixp_modem.c
bt87x.c: bt87x.patch $(SND_TOPDIR)/alsa-kernel/pci/bt87x.c
cmipci.c: cmipci.patch $(SND_TOPDIR)/alsa-kernel/pci/cmipci.c
ens1370.c: ens1370.patch $(SND_TOPDIR)/alsa-kernel/pci/ens1370.c
fm801.c: fm801.patch $(SND_TOPDIR)/alsa-kernel/pci/fm801.c
intel8x0.c: intel8x0.patch $(SND_TOPDIR)/alsa-kernel/pci/intel8x0.c
maestro3.c: maestro3.patch $(SND_TOPDIR)/alsa-kernel/pci/maestro3.c
via82xx.c: via82xx.patch $(SND_TOPDIR)/alsa-kernel/pci/via82xx.c
via82xx_modem.c: via82xx_modem.patch $(SND_TOPDIR)/alsa-kernel/pci/via82xx_modem.c
