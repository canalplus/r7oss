#################################################################################
 # Ralink Technology, Inc.	                                         	#
 # 4F, No. 2 Technology 5th Rd.                                          	#
 # Science-based Industrial Park                                         	#
 # Hsin-chu, Taiwan, R.O.C.                                              	#
 #                                                                       	#
 # (c) Copyright 2005, Ralink Technology, Inc.                           	#
 #                                                                       	#
 # All rights reserved. Ralink's source code is an unpublished work and the	#
 # use of a copyright notice does not imply otherwise. This source code		#
 # contains confidential trade secret material of Ralink Tech. Any attempt	#
 # or participation in deciphering, decoding, reverse engineering or in any	#
 # way altering the source code is stricitly prohibited, unless the prior	#
 # written consent of Ralink Technology, Inc. is obtained.			#
#################################################################################


#
# Please specify CHIP & INTERFACE Type first
#
# CONFIG_CHIP_NAME = 2880 (PCI, MII)  
#                    3052 (USB, MII)
#                    3662 (USB, MII, PCI, PCIE)
#                    3883 (USB, MII, PCI, PCIE)                   
#                    3352 (USB, MII)
#                    5350 (USB, MII)
#
# CONFIG_INF_TYPE = PCI
#                   MII
#                   USB
#                   PCIE
#                   
#
CONFIG_CHIP_NAME = 3662
CONFIG_INF_TYPE=USB

#
# Please enable CONFIG_EXTRA_CFLAG=y on 2.6.25 or above 
#
CONFIG_EXTRA_CFLAG=y

#
# Please enable CONFIG_RALINK_SRC=y on 2.6.0 or above to configure host driver source code path
#
CONFIG_RALINK_SRC=y

#
# Feature Support
# Aggregation_Enable(USB only), PhaseLoadCode_Enable, RetryPktSend_Enable(MII only)
#
Aggregation_Enable=
ifeq ($(CONFIG_INF_TYPE), MII)
RetryPktSend_Enable=y
PhaseLoadCode_Enable=
endif



CONFIG_NM_SUPPORT=
CONFIG_MC_SUPPORT=
CONFIG_MESH_SUPPORT=
CONFIG_WIDI_SUPPORT=

#
# Chipset Related Feature Support
# CONFIG_CONCURRENT_INIC_SUPPORT (RT3883/RT3662 only)
# CONFIG_NEW_MBSS_SUPPORT (RT3883/RT3662 only)
#


ifeq ($(CONFIG_CHIP_NAME), $(filter 3662 3883, $(CONFIG_CHIP_NAME)))
CONFIG_CONCURRENT_INIC_SUPPORT=
CONFIG_NEW_MBSS_SUPPORT=
endif


#CONFIG_WOWLAN_SUPPORT
CONFIG_WOWLAN_SUPPORT=

# some platform not support 32-bit DMA addressing, need to turn this flag on
CONFIG_PCI_FORCE_DMA=

#PLATFORM = FREESCALE832X
#PLATFORM = FREESCALE8377
PLATFORM = ST
#PLATFORM = IXP2
#PLATFORM = TWINPASS
#PLATFORM = 5VT
#PLATFORM = STAR
#PLATFORM = VITESSE
#PLATFORM = RDC
#PLATFORM = SIGMA
#PLATFORM = BRCM
#PLATFORM = IKANOS_VX160
#PLATFORM = IKANOS_VX180
#PLATFORM = PIKA
#PLATFORM = MNDSPEED
#PLATFORM = MNDSPEED2
#PLATFORM = INF_AMAZON_SE
#PLATFORM = RT288x
#PLATFORM = UBICOM6622

ifeq ($(PLATFORM),ST)
LINUX_SRC = $(KERNELDIR)
CROSS_COMPILE = sh4-stlinux23-linux-gnu-
ARCH := sh
export ARCH
endif

ifeq ($(PLATFORM),UBICOM32)
RALINK_SRC = $(shell pwd)
else
ifeq ($(CONFIG_RALINK_SRC), y)
RALINK_SRC = $(src)
else
RALINK_SRC = $(PWD)
endif
endif


ifeq ($(PLATFORM),PC)
#
# Linux 2.6
#
LINUX_SRC = /lib/modules/$(shell uname -r)/build
#
# Linux 2.4 Change to your local setting
#
#LINUX_SRC = /usr/src/linux-2.4
#CFLAGS := -D__KERNEL__ -I$(LINUX_SRC)/include -O2 -fomit-frame-pointer -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i686 -DMODULE -DMODVERSIONS -include $(LINUX_SRC)/include/linux/modversions.h $(WFLAGS)
CROSS_COMPILE =
endif

ifeq ($(PLATFORM),FREESCALE832X)
LINUX_SRC = /opt/ltib-mpc832x_rdb-20070420/rpm/BUILD/linux-2.6.20/ 
CROSS_COMPILE = /opt/freescale/usr/local/gcc-4.0.2-glibc-2.3.6-nptl-2/powerpc-e300c2-linux/bin/powerpc-e300c2-linux-
ARCH = powerpc
endif

ifeq ($(PLATFORM),FREESCALE8377)
LINUX_SRC = /opt/ltib-mpc8377_rds-20090309/rpm/BUILD/linux-2.6.25/ 
CROSS_COMPILE = /opt/freescale/usr/local/gcc-4.2.187-eglibc-2.5.187/powerpc-linux-gnu/bin/powerpc-linux-gnu-
ARCH = powerpc
endif

ifeq ($(PLATFORM),RT288x)
LINUX_SRC = /opt/RT288x_SDK/source/linux-2.6.21.x
CROSS_COMPILE = /opt/buildroot-gcc342/bin/mipsel-linux-
endif

ifeq ($(PLATFORM),SIGMA)
LINUX_SRC = /opt/sigma/kernel/smp86xx_kernel_source_2.7.172.0/linux-2.6.15
#LINUX_SRC = /home/snowpin/Sigma/SMP863x/172/smp86xx_kernel_source_2.7.172.0/linux-2.6.15
CROSS_COMPILE = /opt/sigma/toolchain/smp86xx_toolchain_2.7.172.0/build_mipsel_nofpu/staging_dir/bin/mipsel-linux-
#CROSS_COMPILE = /home/snowpin/Sigma/SMP863x/172/smp86xx_toolchain_2.7.172.0/build_mipsel_nofpu/staging_dir/bin/mipsel-linux-
endif

ifeq ($(PLATFORM),5VT)
LINUX_SRC = /home/5VT_SDK/linux-2.6.17
CROSS_COMPILE = /opt/crosstool/uClibc_v5te_le_gcc_4_1_1/bin/arm-linux-
endif

ifeq ($(PLATFORM),MNDSPEED)
LINUX_SRC = /opt/Vitesse/linux-2.6-grocx
CROSS_COMPILE = /hd2/mndspeed/tools/bin/arm-linux-
endif

ifeq ($(PLATFORM),MNDSPEED2)
LINUX_SRC = /hd2/mndspeed2/linux-2.6.21.1
CROSS_COMPILE = /hd2/mndspeed2/staging_dir_arm/bin/arm-linux-
MAKE_FLAGS:=ARCH="arm" 
endif

ifeq ($(PLATFORM),RDC)
# Linux 2.6
#LINUX_SRC = /lib/modules/$(shell uname -r)/build
# Linux 2.4 Change to your local setting
LINUX_SRC = /home/RDC/linux_2.4.25
CFLAGS := -I$(LINUX_SRC)/include -O2 -fomit-frame-pointer -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i386 -nostdinc -iwithprefix include -DKBUILD_BASEN -DMODULE -DMODVERSIONS $(WFLAGS)
CROSS_COMPILE =/export/tools/bin/i386-linux-
endif


ifeq ($(PLATFORM),STAR)
LINUX_SRC = /opt/star/kernel/linux-2.4.27-star
CROSS_COMPILE = /opt/star/tools/arm-linux/bin/arm-linux-
CFLAGS := -I$(LINUX_SRC)/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -Uarm -fno-common -pipe -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4  -mshort-load-bytes -msoft-float -Uarm -DMODULE -DMODVERSIONS -include $(LINUX_SRC)/include/linux/modversions.h 
endif

ifeq ($(PLATFORM),VITESSE)
LINUX_SRC = /opt/Vitesse/linux-2.6-grocx
CROSS_COMPILE = /opt/Vitesse/arm-uclibc-3.4.6/bin/arm-linux-
CFLAGS := -I$(LINUX_SRC)/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -Uarm -fno-common -pipe -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4  -msoft-float -Uarm -DMODULE -DMODVERSIONS 
endif

ifeq ($(PLATFORM),IXP)
LINUX_SRC = /hd2/project/stable/MonteJade/IXP425FromJohn/snapgear/linux-2.4.x
CROSS_COMPILE = /hd2/project/bin/arm-linux-
CFLAGS := -mbig-endian -D__KERNEL__ -I$(LINUX_SRC)/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -Uarm -fno-common -pipe -mapcs-32 -D__LINUX_ARM_ARCH__=4 -Wa,-mxscale -mtune=strongarm -malignment-traps  -msoft-float -Uarm  -DUTS_MACHINE='"arm"' -DMODULE -DMODVERSIONS -include $(LINUX_SRC)/include/linux/modversions.h
LDFLAGS += -EB
endif

ifeq ($(PLATFORM),IXP2)
LINUX_SRC = /hd2/kamikaze/8.09.2/build_dir/linux-ixp4xx_generic/linux-2.6.26.8
CROSS_COMPILE = /hd2/kamikaze/8.09.2/staging_dir/toolchain-armeb_gcc4.1.2/bin/armeb-linux-
_CFLAGS := -Os -pipe -march=armv5te -mtune=xscale -funit-at-a-time -fhonour-copts -msoft-float
MAKE_FLAGS:=ARCH="arm" 
endif


ifeq ($(PLATFORM),TWINPASS)
LINUX_SRC = /opt/twinpass/release/2.0.1/source/kernel/opensource/linux-2.4.31
CROSS_COMPILE = /opt/uclibc-toolchain/ifx-lxdb-1-1/gcc-3.3.6/toolchain-mips/bin/mips-linux-
CFLAGS := -DMODULE -DTWINPASS -I$(LINUX_SRC)/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -DIFX_SMALL_FOOTPRINT -I$(LINUX_SRC)/include/asm/gcc -G 0 -mno-abicalls -fno-pic -pipe  -finline-limit=100000 -mabi=32 -march=mips32 -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -Wa,--trap  -mlong-calls  -nostdinc -iwithprefix include 
endif

ifeq ($(PLATFORM),BRCM)
BRCM_ROOT = /opt/BRCM
LINUX_SRC = $(BRCM_ROOT)/kernel/linux
CROSS_COMPILE = $(BRCM_ROOT)/toolchains/uclibc-crosstools/bin/mips-linux-uclibc-
CFLAGS  += -Wall -DMODULE -DBCM63XX -D__MIPSEB__ -mabi=32 -march=mips32 -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -Wa,--trap -fomit-frame-pointer -fno-common -mlong-calls -mno-abicalls -fno-pic -pipe -G 0 -I$(LINUX_SRC)/include/asm-mips/mach-bcm963xx  -I$(LINUX_SRC)/include/asm-mips/mach-generic -I$(LINUX_SRC)/include
export KERNELDIR CROSS_COMPILE CC AS LD
endif


ifeq ($(PLATFORM),IKANOS_VX160)
LINUX_SRC = /opt/ikanos/linux-2.6.18
#CROSS_COMPILE = /opt/ikanos/buildroot/build_mips_nofpu/staging_dir/bin/mips-linux-
CROSS_COMPILE = mips-linux-
CFLAGS := -D__KERNEL__ -DIKANOS_VX_1X0 -I$(LINUX_SRC)/include -I$(LINUX_SRC)/include/asm/gcc -I$(LINUX_SRC)/include/asm-mips/mach-tango2 -I$(LINUX_SRC)/include/asm-mips/mach-tango2 -I$(LINUX_SRC)/include/asm-mips/mach-generic -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -O2 -fomit-frame-pointer -G 0 -mno-abicalls -fno-pic -pipe -march=lx4189 -Wa, -DMODULE $(WFLAGS) 
PLATFORM_OBJ := platform/vr_ikans.o
endif


ifeq ($(PLATFORM),IKANOS_VX180)
LINUX_SRC = /opt/LX_BSP_VX180_5_4_0r1_ALPHA_26DEC07/linux-2.6.18
#CROSS_COMPILE = /opt/ikanos/buildroot/build_mips_nofpu/staging_dir/bin/mips-linux-uclibc
CROSS_COMPILE = mips-linux-uclibc-
CFLAGS := -D__KERNEL__  -DIKANOS_VX_1X0 -mips32r2 -I$(LINUX_SRC)/include -I$(LINUX_SRC)/include/asm/gcc -I$(LINUX_SRC)/include/asm-mips/mach-tango2 -I$(LINUX_SRC)/include/asm-mips/mach-tango2 -I$(LINUX_SRC)/include/asm-mips/mach-generic -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -O2 -fomit-frame-pointer -G 0 -mno-abicalls -fno-pic -pipe -Wa, -DMODULE $(WFLAGS)
PLATFORM_OBJ := platform/vr_ikans.o
endif

ifeq ($(PLATFORM),PIKA)
KSRC = /opt/alpha_pika/linux-2.4.22
LINUX_SRC = /opt/alpha_pika/linux-2.4.22
CROSS_COMPILE = /opt/toolchain-89/bin/arm-elf-
CFLAGS := -D__KERNEL__ -I$(KSRC)/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -Uarm -fno-common -pipe -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4  -mshort-load-bytes -msoft-float -Uarm -DMODULE -DMODVERSIONS -include $(KSRC)/include/linux/modversions.h
endif

ifeq ($(PLATFORM),INF_AMAZON_SE)
# Linux 2.6
#LINUX_SRC = /lib/modules/$(shell uname -r)/build
# Linux 2.4 Change to your local setting
LINUX_SRC = /project/Infineon/ase_rt-3.6.10.4/source/kernel/opensource/linux-2.4.31
#CROSS_COMPILE = mips-linux-
CROSS_COMPILE = /opt/uclibc-toolchain-lxdb/uclibc-toolchain/ifx-lxdb-1-3/gcc-3.3.6/toolchain-mips/mips-linux-uclibc/bin/
WFLAGS += -DINF_AMAZON_SE
CFLAGS := -D__KERNEL__ -DMODULE=1 -I$(LINUX_SRC)/include -I$(RT28xx_DIR)/include -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -DCONFIG_IFX_ALG_QOS -DCONFIG_WAN_VLAN_SUPPORT -fomit-frame-pointer -DIFX_PPPOE_FRAME -G 0 -fno-pic -mno-abicalls -mlong-calls -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -Wa,--trap -nostdinc -iwithprefix include $(WFLAGS)
endif

ifeq ($(PLATFORM),UBICOM32)
CONFIG_EXTRA_CFLAG=y
LINUX_SRC = $(ROOTDIR)/$(LINUXDIR)
CROSS_COMPILE = ubicom32-elf-
WFLAGS += -DRT_BIG_ENDIAN
endif

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AS := $(CROSS_COMPILE)as

_CFLAGS += -I$(RALINK_SRC)/comm
_CFLAGS += -D__KERNEL__ 
_CFLAGS += -O0 -g -Wall -Wstrict-prototypes -Wno-trigraphs 
_CFLAGS += -DDBG -DFIX_POTENTIAL_BUG #-DINBAND_DEBUG
_CFLAGS += $(WFLAGS)

PCI_OBJS := pci/rt_pci_dev.o
MII_OBJS := mii/rt_mii_dev.o
MESH_OBJS := mesh/mesh.o
WIDI_OBJS := widi/widi.o
USB_OBJS := usb/rt_usb_dev.o usb/rt_usb_fwupload.o
COMM_OBJS:= comm/rt_profile.o comm/raconfig.o comm/iwhandler.o \
            comm/ioctl.o comm/iwreq_stub.o comm/mbss.o comm/wds.o \
            comm/apcli.o comm/crc32.o 

obj-m := rt$(CONFIG_CHIP_NAME)_iNIC.o
rt$(CONFIG_CHIP_NAME)_iNIC-objs := $(COMM_OBJS) $(PLATFORM_OBJ)

ifeq ($(CONFIG_NM_SUPPORT), y)
_CFLAGS += -DNM_SUPPORT
endif
ifeq ($(CONFIG_MC_SUPPORT), y)
_CFLAGS += -DMULTIPLE_CARD_SUPPORT
endif

ifeq ($(CONFIG_CONCURRENT_INIC_SUPPORT), y)
_CFLAGS += -DCONFIG_CONCURRENT_INIC_SUPPORT
endif

ifeq ($(CONFIG_NEW_MBSS_SUPPORT), y)
_CFLAGS += -DNEW_MBSS_SUPPORT
endif

ifeq ($(CONFIG_WOWLAN_SUPPORT), y)
_CFLAGS += -DWOWLAN_SUPPORT
endif



ifeq ($(CONFIG_MESH_SUPPORT), y)
rt$(CONFIG_CHIP_NAME)_iNIC-objs += $(MESH_OBJS)
_CFLAGS += -I$(RALINK_SRC)/mesh -DMESH_SUPPORT
endif

ifeq ($(CONFIG_WIDI_SUPPORT), y)
rt$(CONFIG_CHIP_NAME)_iNIC-objs += $(WIDI_OBJS)
_CFLAGS += -I$(RALINK_SRC)/widi -DWIDI_SUPPORT
endif

_CFLAGS += -DCONFIG_CHIP_NAME=$(CONFIG_CHIP_NAME)

ifeq ($(CONFIG_INF_TYPE), PCIE)
rt$(CONFIG_CHIP_NAME)_iNIC-objs += $(PCI_OBJS)
_CFLAGS += -I$(RALINK_SRC)/pci -DCONFIG_INF_TYPE=INIC_INF_TYPE_PCI -DPCIE_RESET
endif

ifeq ($(CONFIG_INF_TYPE), PCI)
rt$(CONFIG_CHIP_NAME)_iNIC-objs += $(PCI_OBJS)
_CFLAGS += -I$(RALINK_SRC)/pci -DCONFIG_INF_TYPE=INIC_INF_TYPE_PCI
ifeq ($(CONFIG_CHIP_NAME), $(filter 2883, $(CONFIG_CHIP_NAME)))
_CFLAGS += -DPCI_NONE_RESET
endif
endif
 
ifeq ($(CONFIG_PCI_FORCE_DMA), y)
_CFLAGS += -DPCI_FORCE_DMA
endif



ifeq ($(CONFIG_INF_TYPE), MII)
rt$(CONFIG_CHIP_NAME)_iNIC-objs += $(MII_OBJS)
_CFLAGS += -I$(RALINK_SRC)/mii -DCONFIG_INF_TYPE=INIC_INF_TYPE_MII
ifeq ($(RetryPktSend_Enable), y)
_CFLAGS += -DRETRY_PKT_SEND
endif
_CFLAGS += -DMII_SLAVE_STANDALONE
endif
 
ifeq ($(CONFIG_INF_TYPE), USB)
rt$(CONFIG_CHIP_NAME)_iNIC-objs += $(USB_OBJS)
_CFLAGS+= -I$(RALINK_SRC)/usb -DCONFIG_INF_TYPE=INIC_INF_TYPE_USB
ifeq ($(CONFIG_CHIP_NAME), $(filter 3662 3883 3352 5350, $(CONFIG_CHIP_NAME)))
_CFLAGS += -DRLK_INIC_USBDEV_GEN2
endif
ifeq ($(Aggregation_Enable), y)
ifeq ($(CONFIG_CHIP_NAME), $(filter 3662 3883 3352 5350, $(CONFIG_CHIP_NAME)))
_CFLAGS += -DRLK_INIC_TX_AGGREATION_ONLY
else
_CFLAGS += -DRLK_INIC_SOFTWARE_AGGREATION
endif
endif
else
ifeq ($(PhaseLoadCode_Enable), y)
_CFLAGS += -DPHASE_LOAD_CODE
endif
endif 

ifneq ($(PLATFORM_OBJ), )
_CFLAGS += -I$(RALINK_SRC)/platform
endif

ifeq ($(CONFIG_EXTRA_CFLAG), y)
EXTRA_CFLAGS += $(_CFLAGS)
else
CFLAGS += $(_CFLAGS)
endif

export LINUX_SRC CROSS_COMPILE CFLAGS EXTRA_CFLAGS

ifneq (,$(findstring 2.4,$(LINUX_SRC)))
	#linux 2.4
all: $(obj-m)
ifneq ($(subdir-m), )
	for d in $(subdir-m);                 \
	do                                    \
	  $(MAKE) --directory=$(subdir-m) all;   \
	done
endif

$(obj-m): $(rt$(CONFIG_CHIP_NAME)_iNIC-objs)
	$(LD) $(LDFLAGS) -r $^ -o $@ -g -G 0 -n -Map rt$(CONFIG_CHIP_NAME)_iNIC.map	
else
	#linux 2.6
all: 
ifeq ($(PLATFORM),$(filter FREESCALE832X FREESCALE8377, $(PLATFORM)))
	$(MAKE) -C $(LINUX_SRC) ARCH=$(ARCH) SUBDIRS=$(PWD) modules
else
ifeq ($(PLATFORM),UBICOM32)
	$(MAKE) -C $(LINUX_SRC) SUBDIRS=$(RALINK_SRC) $(MAKE_FLAGS) modules
else 
	$(MAKE) -C $(LINUX_SRC) SUBDIRS=$(PWD) $(MAKE_FLAGS) modules
endif
endif
endif

ifeq ($(PLATFORM),BRCM)
	cp ./firmware/iNIC_ap.bin $(BRCM_ROOT)/targets/fs.src/lib/
	cp ./firmware/iNIC_ap.dat $(BRCM_ROOT)/targets/fs.src/lib/
	cp ./rt$(CONFIG_CHIP_NAME)_iNIC.ko $(BRCM_ROOT)/targets/fs.src/lib/
endif
	

ifeq ($(PLATFORM),UBICOM32)
romfs:
	mkdir -p $(ROMFSDIR)/lib/modules
	cp -f *.ko $(ROMFSDIR)/lib/modules
endif


clean:
	rm -f $(COMM_OBJS) $(MII_OBJS) $(PCI_OBJS) $(PLATFORM_OBJS) $(USB_OBJS) $(MESH_OBJS) $(WIDI_OBJS) Module.symvers *~ .*.cmd *.map *.o *.ko *.mod.c comm/.*.cmd pci/.*.cmd mii/.*.cmd widi/.*.cmd usb/.*.cmd platform/.*.cmd *.order
	for d in $(subdir-m); \
    do                       \
      $(MAKE) --directory=$$d $@; \
    done


