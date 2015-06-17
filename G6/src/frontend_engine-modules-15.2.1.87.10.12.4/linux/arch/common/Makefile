#
# Makefile for common board setup source files
#

ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/


ifdef CONFIG_SH_ST_HDK7108
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/arch/sh/boards/mach-hdk7108
STM_FE_PLATFORM:=y
endif

ifdef CONFIG_MACH_STM_B2092
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/arch/arm/mach-stm-b2092/
STM_FE_PLATFORM:=y
endif

ifdef CONFIG_SH_ST_ADI7108
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/arch/sh/boards/mach-adi7108
STM_FE_PLATFORM:=y
endif

ifdef CONFIG_MACH_STM_B2044
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/arch/arm/mach-stm-b2044/
STM_FE_PLATFORM:=y
endif


#source objects
stm_fe_platform-objs = stm_fe_setup.o \
			config_sysfs.o

#target kernel module
ifdef STM_FE_PLATFORM
obj-m += stm_fe_platform.o
endif
