# Objects we will link into busybox / subdirs we need to visit

CONFIG_KERNEL_BUILD?=$(KERNELDIR)
CONFIG_KERNEL_PATH?=$(KERNELDIR)

TREE_ROOT := $(shell pwd)

export CONFIG_KERNEL_BUILD CONFIG_KERNEL_PATH TREE_ROOT CONFIG_PLAYER2_PATH CONFIG_STGFB_PATH \
       CONFIG_BLITTER_PATH CONFIG_MULTICOM_PATH CONFIG_INFRASTRUCTURE_PATH
export TARGET_DIR


all: modules

distclean clean:
	-find . -name "*.o" | xargs rm -f
	-find . -name ".*.o.cmd" | xargs rm -f
	-find . -name ".*.ko.cmd" | xargs rm -f
	-find . -name "*.ko" | xargs rm -f
	-find . -name "*.mod.c" | xargs rm -f
	rm -rf doxygen
	rm -rf linux/.tmp_versions/*

KBUILD_PATH := $(shell pwd)/linux
ifeq ($(CONFIG_STM_VIRTUAL_PLATFORM),) # VSoC arch for compilation must be set to HCE
ifneq ($(STM_TARGET_CPU),)
MYARCH := $(STM_TARGET_CPU)
else
ifneq ($(ARCH),)
MYARCH := $(ARCH)
else
MYARCH := sh
endif
endif
else
MYARCH := hce
endif

headers_install:
	mkdir -p $(INSTALL_HDR_PATH)/linux
	cp -Lr $(TREE_ROOT)/linux/include/linux $(INSTALL_HDR_PATH)

modules:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M="$(KBUILD_PATH)" CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) modules

modules_install:
	$(MAKE) -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M="$(KBUILD_PATH)" CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(MYARCH) INSTALL_MOD_DIR=stlinuxtv modules_install

docs doxygen : .PHONY
	doxygen

.PHONY :
