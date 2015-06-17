
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


ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl584/mxl584_lla

ccflags-y+= -g

ccflags-y+=-DCHIP_STAPI
#source objects

# -- SAT MXL HYDA 584 driver set --
mxl584_lla-objs = 	d0mxl584.o \
			mxl584_drv.o \
			mxl584_init.o \
			mxl584_lla/mxlware_fw_download.o \
			mxl584_lla/mxlware_hydra_bertapi.o \
			mxl584_lla/mxlware_hydra_commonapi.o \
			mxl584_lla/mxlware_hydra_demodtunerapi.o \
			mxl584_lla/mxlware_hydra_deviceapi.o \
			mxl584_lla/mxlware_hydra_diseqcfskapi.o \
			mxl584_lla/mxlware_hydra_oem_drv.o \
			mxl584_lla/mxlware_hydra_phyctrl.o \
			mxl584_lla/mxlware_hydra_serdesapi.o \
			mxl584_lla/mxlware_hydra_tsmuxctrlapi.o

#target kernel module
obj-m += mxl584_lla.o
