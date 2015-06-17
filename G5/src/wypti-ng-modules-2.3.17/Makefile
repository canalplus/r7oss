# Objects we will link into busybox / subdirs we need to visit

# Genbox-ng bug
LDFLAGS=

KERNELDIR ?= $(KERNEL_DIR)
CONFIG_KERNEL_BUILD?=$(KERNEL_DIR)
CONFIG_KERNEL_PATH?=$(KERNEL_DIR)
CONFIG_MODULES_PATH?=$(ROOT)

TREE_ROOT := $(PWD)/

export CONFIG_KERNEL_BUILD CONFIG_KERNEL_PATH CONFIG_STGFB_PATH CONFIG_MODULES_PATH TREE_ROOT

##dir-y += linux/


modules:
	make -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=$(TREE_ROOT)/linux modules

modules_install:
	make -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=$(TREE_ROOT)/linux modules_install

clean:
	-rm -rf linux/.tmp_versions linux/Module.symvers
	make -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=$(TREE_ROOT)/linux clean

all: clean modules modules_install

