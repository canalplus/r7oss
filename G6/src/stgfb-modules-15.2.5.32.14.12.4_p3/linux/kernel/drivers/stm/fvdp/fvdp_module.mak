EXTRA_CXXFLAGS := -fno-rtti -fno-exceptions

SRC_TOPDIR := ../../../../../..

EXTRA_CXXFLAGS += -include $(STG_TOPDIR)/linux/kernel/drivers/stm/coredisplay/linux-platform.h

ifneq ($(CONFIG_INFRASTRUCTURE_PATH),)
EXTRA_CXXFLAGS += -DCONFIG_INFRASTRUCTURE -I $(CONFIG_INFRASTRUCTURE_PATH)/include
endif

EXTRA_CFLAGS += -I$(STG_TOPDIR)/linux/kernel/drivers/stm/fvdp

STM_SRC_FILES := $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/fvdp/, \
  fvdp_module.c)

include $(STG_TOPDIR)/fvdp/fvdp.mak


ifneq ($(CONFIG_STG_OPTLEVEL),)
EXTRA_CXXFLAGS += -O$(CONFIG_STG_OPTLEVEL)
EXTRA_CFLAGS   += -O$(CONFIG_STG_OPTLEVEL)
# For C file including linux header, compilation is failing when changing option level
# so keep default option level for them.
CFLAGS_REMOVE_fvdp_module.o += -O$(CONFIG_STG_OPTLEVEL)
endif

EXTRA_CFLAGS += -Werror

# Extract and Provide Version Information from git if available
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
FVDP_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)

# If our git-describe is unsuccessful, we should fall back to an RPMBuild generated file
ifeq ($(FVDP_VERSION),)
FVDP_VERSION := $(shell cat $(STG_TOPDIR)/sdk2pkg/product_version)
endif

# Add build information defines for just the fvdp object.
CFLAGS_fvdp_module.o += -DKBUILD_VERSION="KBUILD_STR($(FVDP_VERSION))"


