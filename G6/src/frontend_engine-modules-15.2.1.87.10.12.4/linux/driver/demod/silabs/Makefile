
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


ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs/silabs_lla/tuner/SiLabs_TER_Tuner
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs/silabs_lla/tuner/Si2147
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs/silabs_lla/i2c
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs/silabs_lla/i2c/Si_I2C
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs/silabs_lla/demod/Si21652B

ccflags-y+= -g

ccflags-y+=-DCHIP_STAPI

#Required by SiLabs drivers
ccflags-y+=-DNO_SAT -DDEMOD_Si21652B -DSi21652B_22_COMPATIBLE -DTER_TUNER_SILABS -DTER_TUNER_Si2147 -DINDIRECT_I2C_CONNECTION -DNO_WIN32 -DFRONT_END_COUNT=2 -DNO_FLOATS_ALLOWED
#TCH customization of SiTRACES (else no extended traces)
#ccflags-y+=-DTCH_CUST_SiTRACES
#Option enabling to activate SiLabs L0 test (time and I2C)
#ccflags-y+=-DTCH_SILABS_L0_TEST

#source objects
silabs_lla-objs = silabs_lla_int.o

#target kernel module
obj-m += silabs_lla.o
