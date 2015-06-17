
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner/tda18212
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner
ccflags-y+=-DCHIP_STAPI

#source objects
tda18212_lla-objs = tmddtda18212instance.o \
		    tmddtda18212_advanced.o \
		    tmddtda18212.o \
		    tmbsltda18212instance.o \
		    tmbsltda18212.o \
		    tda18212_tuner.o \
		    tda18212_install.o
#target kernel module

obj-m += tda18212_lla.o
