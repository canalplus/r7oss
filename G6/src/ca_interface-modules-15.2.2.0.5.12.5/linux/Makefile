# Get the source directory with symlinks resolved

BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)

EXTRA_CFLAGS+=-I$(INFRASTRUCTURE)/include/
export STM_CA_TOPDIR := $(shell cd $(BUILD_SOURCE_PATH) && readlink -e ../)

obj-y += scr/
obj-y += ci/
