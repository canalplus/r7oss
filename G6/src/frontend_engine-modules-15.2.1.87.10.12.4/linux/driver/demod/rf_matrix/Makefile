
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv090x
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0910
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0900
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/lnb

#source objects
stm_fe_rf_matrix_stv6120-objs = rf_matrix_stv6120.o

#target kernel module
ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV6120)))
obj-m += stm_fe_rf_matrix_stv6120.o
endif
