EXTRA_CFLAGS += -I$(STG_TOPDIR)/private/include/

EXTRA_CXXFLAGS := -fno-rtti -fno-exceptions

SRC_TOPDIR := ../../../../../..

EXTRA_CXXFLAGS += -include $(STG_TOPDIR)/linux/kernel/include/linux/stm/linux-platform.h

ifneq ($(CONFIG_INFRASTRUCTURE_PATH),)
EXTRA_CXXFLAGS += -DCONFIG_INFRASTRUCTURE -I $(CONFIG_INFRASTRUCTURE_PATH)/include
endif

include $(STG_TOPDIR)/hdmirx/hdmirx.mak
include $(STG_TOPDIR)/hdmirx/soc/soc.mak

EXTRA_CFLAGS += \
   $(CFLAGS_HDRX) \
   -I$(INFRA_PATH)/include \
   -I$(STMFB_PATH)/include \
   -I$(KERNEL_ROOT)/include/linux/stm


STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/hdmirx/, \
  hdmirxdrv.c \
  hdmirxutils.c)

ifeq (y, $(CONFIG_SYSFS))
ifeq (y, $(filter y, $(SDK2_ENABLE_ATTRIBUTES)$(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)))
EXTRA_CFLAGS += -DSDK2_ENABLE_HDMI_RX_ATTRIBUTES
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/hdmirx/, hdmirxsysfs.c)
endif
endif

ifneq ($(CONFIG_STG_OPTLEVEL),)
EXTRA_CXXFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif

EXTRA_CFLAGS += -Werror

BUILD_SYSTEM_INFO = $(shell /bin/uname -a)
BUILD_USER = $(shell /usr/bin/whoami)
BUILD_DATE = $(shell /bin/date --iso-8601)

STMFB_ORIGINAL_SOURCE_PATH ?= $(shell dirname `readlink -e $(M)/../../Makefile`)

# Extract and Provide Version Information from git if available
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
HDMIRX_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)


# If our git-describe is unsuccessful, we should fall back to an RPMBuild generated file
ifeq ($(HDMIRX_VERSION),)
HDMIRX_VERSION := $(shell cat $(STG_TOPDIR)/sdk2pkg/product_version)
endif

# Add build information defines for just the hdmirx object which will
# appear in sysfs. Because of the time information this file will rebuild
# every time.
CFLAGS_hdmirx.o := -DKBUILD_SYSTEM_INFO="KBUILD_STR($(BUILD_SYSTEM_INFO))"     \
                        -DKBUILD_USER="KBUILD_STR($(BUILD_USER))"                   \
                        -DKBUILD_SOURCE="KBUILD_STR($(STMFB_ORIGINAL_SOURCE_PATH))" \
                        -DKBUILD_VERSION="KBUILD_STR($(HDMIRX_VERSION))"       \
                        -DKBUILD_DATE="KBUILD_STR($(BUILD_DATE))"
