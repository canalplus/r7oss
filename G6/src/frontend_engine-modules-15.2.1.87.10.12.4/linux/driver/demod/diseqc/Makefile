
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv090x
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0910
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0900
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0913
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/lnb
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl582
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl582/mxl582_lla

#source objects
stm_fe_diseqc_stv090x-objs = diseqc_stv090x.o
stm_fe_diseqc_stv0910-objs = diseqc_stv0910.o
stm_fe_diseqc_stv0913-objs = diseqc_stv0913.o
stm_fe_diseqc_stv0900-objs = diseqc_stv0900.o
stm_fe_diseqc_mxl582-objs = diseqc_mxl582.o

#target kernel module
ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV090X)))
obj-m += stm_fe_diseqc_stv090x.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV0910)))
obj-m += stm_fe_diseqc_stv0910.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV0900)))
obj-m += stm_fe_diseqc_stv0900.o
endif

ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_STV0913)))
obj-m += stm_fe_diseqc_stv0913.o
endif
ifeq (y, $(strip $(CONFIG_FRONTEND_ENGINE_MXL582)))
obj-m += stm_fe_diseqc_mxl582.o
endif
