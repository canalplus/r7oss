##############################################################################################################
# Prerequist : CONFIG_KERNEL_PATH have to be kernel source directrory.
#              CONFIG_KERNEL_BUILD have to be compilation directory of the kernel (KBUILD_OUTPUT or O)
#              CROSS_COMPILE
#              CONFIG_MODULES_PATH where to install the built .ko.
##############################################################################################################


KBUILD_PATH := $(shell pwd)/linux
ifneq ($(STM_TARGET_CPU),)
MYARCH := $(STM_TARGET_CPU)
else
ifneq ($(ARCH),)
MYARCH := $(ARCH)
else
MYARCH := sh
endif
endif

# Extract and Provide Version Information
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
STM_FE_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)
# We only define STM_FE_VERSION to the source tree if we were successful from git-describe
ifneq ($(STM_FE_VERSION),)
EXTRA_CFLAGS += -DSTM_FE_VERSION="KBUILD_STR($(STM_FE_VERSION))"
endif

INFRA_POWER_SCRIPT=./stm_power/scripts
all: modules

install: modules_install

modules:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/linux modules CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH)
ifeq ($(INFRA_BUILD_TEST),1)
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/tests  modules CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH)
endif

modules_install:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD)  M=`pwd`/linux modules_install CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) INSTALL_MOD_PATH=$(CONFIG_MODULES_PATH) INSTALL_MOD_DIR=infrastructure
ifeq ($(INFRA_BUILD_TEST),1)
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD)  M=`pwd`/tests modules_install CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) INSTALL_MOD_PATH=$(CONFIG_MODULES_PATH) INSTALL_MOD_DIR=infrastructure
endif
ifeq ($(CONFIG_PM),y)
	mkdir -p $(CONFIG_MODULES_PATH)/infra_power
	cp $(INFRA_POWER_SCRIPT)/*.txt  $(CONFIG_MODULES_PATH)/infra_power
	cp $(INFRA_POWER_SCRIPT)/*.sh  $(CONFIG_MODULES_PATH)/infra_power
endif

infrastructure_tests:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/tests  modules CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH)

infrastructure_tests_install:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/tests modules_install CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) INSTALL_MOD_PATH=$(CONFIG_MODULES_PATH) INSTALL_MOD_DIR=infrastructure

infrastructure_tests_clean:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/tests  clean CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH)

distclean: clean
	-find . -name "*.o" | xargs rm -f
	-find . -name ".*.o.cmd" | xargs rm -f
	-find . -name ".*.ko.cmd" | xargs rm -f
	-find . -name "*.ko" | xargs rm -f
	-find . -name "*.mod.c" | xargs rm -f
	rm -rf doxygen
	rm -rf linux/.tmp_versions/*


