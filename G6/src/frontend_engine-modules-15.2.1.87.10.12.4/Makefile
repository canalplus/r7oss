# Objects we will link into busybox / subdirs we need to visit

CONFIG_KERNEL_BUILD?=$(KERNELDIR)
CONFIG_KERNEL_PATH?=$(KERNELDIR)

export CONFIG_KERNEL_BUILD CONFIG_KERNEL_PATH
export INSTALL_MOD_PATH


all: modules

distclean: clean
	-find . -name "*.o" | xargs rm -f
	-find . -name ".*.o.cmd" | xargs rm -f
	-find . -name ".*.ko.cmd" | xargs rm -f
	-find . -name "*.ko" | xargs rm -f
	-find . -name "*.mod.c" | xargs rm -f
	rm -rf doxygen
	rm -rf linux/.tmp_versions/*

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

modules:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M="$(KBUILD_PATH)" CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) modules

modules_install:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M="$(KBUILD_PATH)" CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) INSTALL_MOD_DIR=frontend_engine modules_install

docs doxygen : .PHONY
	doxygen

.PHONY :
