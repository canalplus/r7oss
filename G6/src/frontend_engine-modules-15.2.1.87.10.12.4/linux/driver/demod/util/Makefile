
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-DCHIP_STAPI

#source objects
stm_fe_util-objs = i2c_wrapper.o \
                  stfe_utilities.o \
                  lla_utils.o

#target kernel module
obj-m += stm_fe_util.o
