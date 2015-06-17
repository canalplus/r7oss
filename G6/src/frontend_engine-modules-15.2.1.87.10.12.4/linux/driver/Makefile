#CFLAGS+= -g
#LDFLAGS+= -g
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

#include path for stm_fe internal files
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/debug
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/util
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv090x
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0913
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0910
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0900
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stv0367
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl584
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl582
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/mxl214
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/silabs
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/dummy_demod
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/remote/pcpd_lla
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/remote/tsserver_lla
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/tuner/tda18212
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/lnb
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/diseqc
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/rf_matrix
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/stats
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/demod/pm
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/ip
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/ip/ipfec
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/ip/stats
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/ip/strm
ccflags-y+= -g

# Extract and Provide Version Information
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
STM_FE_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)
# We only define STM_FE_VERSION to the source tree if we were successful from git-describe
ifneq ($(STM_FE_VERSION),)
ccflags-y += -DSTM_FE_VERSION="KBUILD_STR($(STM_FE_VERSION))"
endif

#source objects
stm_fe-objs = stm_fe_engine.o \
		stm_fe_demod.o \
		stm_fe_lnb.o \
		stm_fe_diseqc.o \
		stm_fe_rf_matrix.o \
		stm_fe_install.o \
		stm_fe_dt_install.o \
		stm_fe_ip.o \
		stm_fe_ipfec.o \
		stm_fe_ip_install.o \
		demod/demod_task.o \
		demod/demod_scan.o \
		demod/stats/stats.o \
		ip/stats/stats.o \
		ip/ipfec/stats/stats.o

ifdef CONFIG_PM
stm_fe-objs += demod/pm/pm.o
endif

stm_fe-$(CONFIG_DEBUG_FS) += demod/debug/debugfs.o

stm_fe-objs += $(stm-fe-y)

#target kernel module
obj-m += stm_fe.o
