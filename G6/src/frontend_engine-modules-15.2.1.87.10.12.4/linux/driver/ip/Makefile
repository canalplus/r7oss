
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper

#source objects
ifdef CONFIG_NETFILTER
stm_fe_ip-objs = fe_ip.o \
		 fe_ip_rtp.o \
		 ipfec/fe_ip_fec.o\
		 strm/fe_ip_strm.o

#target kernel module
obj-m += stm_fe_ip.o
endif
