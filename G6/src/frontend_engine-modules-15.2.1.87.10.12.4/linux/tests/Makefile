# Get the source directory with symlinks resolved
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
export STM_FE_TOPDIR := $(shell cd $(BUILD_SOURCE_PATH) && readlink -e ../../)

ccflags-y+=-I$(STM_FE_TOPDIR)/linux/include/ \
	-I$(STM_FE_TOPDIR)/include

#include paths fo other component's header files
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include/ \
		-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include/

ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver
ccflags-y+=-I$(STM_FE_TOPDIR)/linux/driver/stm_os_wrapper

#source objects
stm_fe_test-objs = main_test.o
stm_fe_utility_test-objs = utility_tests.o
stm_fe_api_test-objs = api_test.o
stm_fe_ip_api_test-objs = ip_api_test.o
stm_fe_ip_api_pcount_test-objs = ip_api_pcount_test.o
stm_fe_ip_api_memsink_test-objs = ip_api_memsink_test.o
stm_fe_scan_test-objs = scan_test.o
#target kernel module
obj-m += stm_fe_test.o
obj-m += stm_fe_utility_test.o
obj-m += stm_fe_api_test.o
obj-m += stm_fe_ip_api_test.o
obj-m += stm_fe_ip_api_pcount_test.o
obj-m += stm_fe_ip_api_memsink_test.o
obj-m += stm_fe_scan_test.o
