
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-DCHIP_STAPI

#source objects
stvvglna_lla-objs = stvvglna.o

#target kernel module

obj-m += stvvglna_lla.o
