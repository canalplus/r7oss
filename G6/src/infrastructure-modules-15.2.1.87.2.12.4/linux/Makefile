# Get the source directory with symlinks resolved
#export STM_INFRA_TOPDIR := $(shell readlink -e ../)

BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
export STM_INFRA_TOPDIR := $(shell cd $(BUILD_SOURCE_PATH) && readlink -e ../)


obj-y += event/
obj-y += wrapper/
obj-y += registry/
obj-y += memsrcsink/
#CONFIG_INFRA_EXTENDED flag controls the compilation of H/W drivers.
#This flag is enabled in teh config.in file of the build system
ifeq ($(CONFIG_INFRA_EXTENDED),y)
ifeq ($(CONFIG_PM),y)
obj-y += power/
endif
obj-y += arch/
#Remove cec from infra build .
#TODO : Files should be deleted once cec is migrated to vibe
#obj-y += cec/
endif
obj-y += gearbox/
