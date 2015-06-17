EXTRA_CXXFLAGS := -fno-rtti -fno-exceptions

SRC_TOPDIR := ../../../../../..

EXTRA_CXXFLAGS += -include $(STG_TOPDIR)/linux/kernel/include/linux/stm/linux-platform.h

ifneq ($(CONFIG_INFRASTRUCTURE_PATH),)
EXTRA_CXXFLAGS += -DCONFIG_INFRASTRUCTURE -I $(CONFIG_INFRASTRUCTURE_PATH)/include
endif

KBUILD_CPPFLAGS += -I$(STG_TOPDIR)/linux/kernel/drivers/stm/vxi

# Common stm-vxi Linux specific module files
DEVICESOURCEFILES := $(addprefix  ../,                                 \
            services.c                                                 \
            vxi.c)

# Base class files and C API implementation
GENERICSRCS := $(addprefix $(SRC_TOPDIR)/vxi/generic/,                 \
			VxiDevice.cpp)

STM_VXI_SRC_FILES := $(DEVICESOURCEFILES) $(GENERICSRCS)

# Pull in all the new IP objects. Eventually a lot of the above will disappear
# into here as well.
include $(STG_TOPDIR)/vxi/ip/ip.mak

ifneq ($(CONFIG_STG_OPTLEVEL),)
EXTRA_CXXFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif

EXTRA_CFLAGS += -Werror

BUILD_SYSTEM_INFO = $(shell /bin/uname -a)
BUILD_USER = $(shell /usr/bin/whoami)
BUILD_DATE = $(shell /bin/date --iso-8601)

CAPTURE_ORIGINAL_SOURCE_PATH ?= $(shell dirname `readlink -e $(M)/../../Makefile`)

# Extract and Provide Version Information from git if available
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
CAPTURE_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)


# If our git-describe is unsuccessful, we should fall back to an RPMBuild generated file
ifeq ($(CAPTURE_VERSION),)
CAPTURE_VERSION := $(shell cat $(STG_TOPDIR)/sdk2pkg/product_version)
endif

# Add build information defines for just the coredisplay object which will
# appear in sysfs. Because of the time information this file will rebuild
# every time.
CFLAGS_class.o := -DKBUILD_SYSTEM_INFO="KBUILD_STR($(BUILD_SYSTEM_INFO))"     \
                        -DKBUILD_USER="KBUILD_STR($(BUILD_USER))"                   \
                        -DKBUILD_SOURCE="KBUILD_STR($(CAPTURE_ORIGINAL_SOURCE_PATH))" \
                        -DKBUILD_VERSION="KBUILD_STR($(CAPTURE_VERSION))"       \
                        -DKBUILD_DATE="KBUILD_STR($(BUILD_DATE))"


# C++ build magic
EXTRA_CFLAGS += -DINSERT_EXTRA_CXXFLAGS_HERE
mould_cxx_cflags = $(subst -ffreestanding,,\
		   $(subst -Wstrict-prototypes,,\
		   $(subst -Wno-pointer-sign,,\
		   $(subst -Wdeclaration-after-statement,,\
		   $(subst -Werror-implicit-function-declaration,,\
		   $(subst -DINSERT_EXTRA_CXXFLAGS_HERE,$(EXTRA_CXXFLAGS),\
		   $(1)))))))


quiet_cmd_cc_o_cpp = CC $(quiet_modtab) $@

cmd_cc_o_cpp = $(call mould_cxx_cflags,$(cmd_cc_o_c))

define rule_cc_o_cpp
	$(call echo-cmd,checksrc) $(cmd_checksrc)                         \
	$(call echo-cmd,cc_o_cpp)                                         \
	$(cmd_cc_o_cpp);                                                  \
	$(cmd_modversions)                                                \
	scripts/basic/fixdep $(depfile) $@ '$(call make-cmd,cc_o_cpp)' > $(@D)/.$(@F).tmp;  \
	rm -f $(depfile);                                                 \
	mv -f $(@D)/.$(@F).tmp $(@D)/.$(@F).cmd
endef

%.o: %.cpp FORCE
	$(call cmd,force_checksrc)
	$(call if_changed_rule,cc_o_cpp)

