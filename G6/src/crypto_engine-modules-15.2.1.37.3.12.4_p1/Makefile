CONFIG_KERNEL_BUILD?=$(KERNELDIR)
CONFIG_KERNEL_PATH?=$(KERNELDIR)

TREE_ROOT := $(shell pwd)

export CONFIG_KERNEL_BUILD CONFIG_KERNEL_PATH TREE_ROOT

J?=-j 1

all: modules

distclean: clean
	-find . -name "*.o" | xargs rm -f
	-find . -name ".*.o.cmd" | xargs rm -f
	-find . -name ".*.ko.cmd" | xargs rm -f
	-find . -name "*.ko" | xargs rm -f
	-find . -name "*.mod.c" | xargs rm -f
	rm -rf doxygen
	rm -rf linux/.tmp_versions/*

KBUILD_PATH := $(CURDIR)/linux
ifneq ($(STM_TARGET_CPU),)
MYARCH := $(STM_TARGET_CPU)
else
ifneq ($(ARCH),)
MYARCH := $(ARCH)
else
MYARCH := sh
endif
endif

headers_install:
	# Firmware specific headers
	cp -Lr $(SDK2_SOURCE_CRYPTO_ENGINE_FW_API_HEADERS)/linux $(INSTALL_HDR_PATH)/usr/include/
	# stm_cipher header
	cp -Lr $(TREE_ROOT)/linux/include/* $(INSTALL_HDR_PATH)/usr/include/

modules:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M="$(KBUILD_PATH)" CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) modules

modules_install:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M="$(KBUILD_PATH)" CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) INSTALL_MOD_DIR=crypto_engine modules_install

docs doxygen : .PHONY
	doxygen

.PHONY :
