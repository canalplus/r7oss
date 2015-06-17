EXTRA_CXXFLAGS := -fno-rtti -fno-exceptions

SRC_TOPDIR := ../../../../../..

EXTRA_CXXFLAGS += -include $(STG_TOPDIR)/linux/kernel/include/linux/stm/linux-platform.h

ifneq ($(CONFIG_INFRASTRUCTURE_PATH),)
EXTRA_CXXFLAGS += -DCONFIG_INFRASTRUCTURE -I $(CONFIG_INFRASTRUCTURE_PATH)/include
endif


STM_SRC_FILES := $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/scaler/, \
  services.c                                                                 \
  scaler_module.c)


include $(STG_TOPDIR)/scaler/generic/generic.mak


ifneq ($(CONFIG_STG_OPTLEVEL),)
EXTRA_CXXFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif

EXTRA_CFLAGS += -Werror

# Extract and Provide Version Information from git if available
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
SCALER_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)

# If our git-describe is unsuccessful, we should fall back to an RPMBuild generated file
ifeq ($(SCALER_VERSION),)
SCALER_VERSION := $(shell cat $(STG_TOPDIR)/sdk2pkg/product_version)
endif

# Add build information defines for just the scaler object.
CFLAGS_scaler_module.o := -DKBUILD_VERSION="KBUILD_STR($(SCALER_VERSION))"


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

