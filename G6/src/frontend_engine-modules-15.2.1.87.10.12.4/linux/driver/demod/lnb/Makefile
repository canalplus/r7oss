
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper


#source objects
stm_fe_lnbh24-objs = lnbh24.o
stm_fe_lnbh26-objs = lnbh26.o

#target kernel module
ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_LNBH24)))
obj-m += stm_fe_lnbh24.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_LNBH26)))
obj-m += stm_fe_lnbh26.o
endif
