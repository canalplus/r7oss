
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper


ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl582/mxl582_lla

ccflags-y+= -g

ccflags-y+=-DCHIP_STAPI
#source objects

# -- SAT MXL HYDA 582 driver set --
mxl582_lla-objs = 	d0mxl582.o \
			mxl582_drv.o \
			mxl582_init.o \
			mxl582_lla/mxlware_fw_download.o \
			mxl582_lla/mxlware_hydra_bertapi.o \
			mxl582_lla/mxlware_hydra_commonapi.o \
			mxl582_lla/mxlware_hydra_demodtunerapi.o \
			mxl582_lla/mxlware_hydra_deviceapi.o \
			mxl582_lla/mxlware_hydra_diseqcfskapi.o \
			mxl582_lla/mxlware_hydra_oem_drv.o \
			mxl582_lla/mxlware_hydra_phyctrl.o \
			mxl582_lla/mxlware_hydra_serdesapi.o \
			mxl582_lla/mxlware_hydra_tsmuxctrlapi.o

#target kernel module
obj-m += mxl582_lla.o
