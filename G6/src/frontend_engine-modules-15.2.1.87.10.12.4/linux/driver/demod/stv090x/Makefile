
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

ccflags-y+=-DCHIP_STAPI
#source objects
stv0903_lla-objs = d0903.o \
				stv0903_drv.o \
				stv0903_init.o

#target kernel module
obj-m += stv0903_lla.o
