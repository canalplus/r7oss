
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-DCHIP_STAPI

#source objects
stv6110_lla-objs = stv6110_install.o \
		   stv6110_tuner.o

stv6120_lla-objs = stv6120_install.o \
		   stv6120_tuner.o

stv6111_lla-objs = stv6111_install.o \
		   stv6111_tuner.o

stv4100_lla-objs = stv4100_install.o \
		   stv4100_tuner.o

dtt7546_lla-objs = dtt7546_install.o \
                   dtt7546_tuner.o

sat_tuner_wrapper-objs = tuner_sat_wrapper.o
tc_tuner_wrapper-objs  = tuner_tc_wrapper.o

#target kernel module
ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV6110)))
obj-m += stv6110_lla.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV6120)))
obj-m += stv6120_lla.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV4100)))
obj-m += stv4100_lla.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_DTT7546)))
obj-m += dtt7546_lla.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV6111)))
obj-m += stv6111_lla.o
endif

obj-m += sat_tuner_wrapper.o
obj-m += tc_tuner_wrapper.o
