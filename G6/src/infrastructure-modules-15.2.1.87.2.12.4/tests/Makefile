
#Ideally this tests dir should be inside ../linux , so that we do not need to define
#STM_INFRA_TOPDIR again here (in addition to linux/Makefile).

BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
export STM_INFRA_TOPDIR := $(shell cd $(BUILD_SOURCE_PATH) && readlink -e ../)

#FLAG DEPENDENT BUILD FOR CODE
INFRA_BUILD_MSS?=0
INFRA_BUILD_CEC?=0
#INFRA_BUILD_CI?=0

obj-y += common/
obj-y += event/
obj-y += registry/
obj-y += memsrcsink/
#obj-y += cec/
obj-y += power/

INFRA_MSSTEST_SCRIPTS=$(STM_INFRA_TOPDIR)/stm_memsrcsink/test/scripts


EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_cec/test/src
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_event/test/src
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_registry/test/src
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_memsrcsink/test/src
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_memsrcsink/test/include
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include

obj-m           += infra_test.o

INFRA_TEST_OBJS :=   infra_load.o

infra_test-objs := $(addprefix infrastructure/, $(INFRA_TEST_OBJS))

copy_scripts:
	$(ECHO) "copy script files"
	mkdir -p $(CONFIG_MODULES_PATH)/infra_msstest
	cp $(INFRA_MSSTEST_SCRIPTS)/*.sh $(CONFIG_MODULES_PATH)/infra_msstest


