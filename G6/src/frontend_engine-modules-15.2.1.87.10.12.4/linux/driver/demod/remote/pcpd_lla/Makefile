
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/remote/tsserver_lla
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper

ccflags-y+=-DCHIP_STAPI
#source objects
remote_lla-objs = d0remote.o remote_util.o

#target kernel module
obj-m += remote_lla.o
