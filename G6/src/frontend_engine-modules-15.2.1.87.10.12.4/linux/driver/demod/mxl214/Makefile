
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


ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl214/mxl214_lla

ccflags-y+= -g

ccflags-y+=-DCHIP_STAPI
#source objects

# -- CAB MXL HRCLS 214 driver set --
mxl214_lla-objs = 	d0mxl214.o \
			mxl214_drv.o \
			mxl214_init.o \
			mxl214_lla/mxl_hrcls_calctrl.o \
			mxl214_lla/mxl_hrcls_commonapi.o \
			mxl214_lla/mxl_hrcls_debugapi.o \
			mxl214_lla/mxl_hrcls_demodapi.o \
			mxl214_lla/mxl_hrcls_oem_drv.o \
			mxl214_lla/mxl_hrcls_oobapi.o \
			mxl214_lla/mxl_hrcls_phyctrl.o \
			mxl214_lla/mxl_hrcls_serdesapi.o \
			mxl214_lla/mxl_hrcls_xptapi.o

#target kernel module
obj-m += mxl214_lla.o
