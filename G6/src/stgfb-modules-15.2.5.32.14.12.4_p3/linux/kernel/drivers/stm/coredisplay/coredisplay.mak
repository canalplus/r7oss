EXTRA_CXXFLAGS := -fno-rtti -fno-exceptions -D__BUILD_COREDISPLAY__

SRC_TOPDIR := ../../../../../..

EXTRA_CXXFLAGS += -include $(STG_TOPDIR)/linux/kernel/include/linux/stm/linux-platform.h

ifneq ($(CONFIG_INFRASTRUCTURE_PATH),)
EXTRA_CXXFLAGS += -DCONFIG_INFRASTRUCTURE -I $(CONFIG_INFRASTRUCTURE_PATH)/include
endif

# Common stmcoredisplay Linux specific module files
CORESOURCEFILES := $(addprefix  ../,                                    \
			services.c                                       \
			coredisplay.c)

# Base class files and C API implementation
GENINITSRCS := $(addprefix $(SRC_TOPDIR)/display/generic/,                     \
			DisplayDevice.cpp                                      \
			DisplayPlane.cpp                                       \
			Node.cpp                                               \
			ControlNode.cpp                                        \
			ListenerNode.cpp                                       \
			DisplayQueue.cpp                                       \
			DisplayNode.cpp                                        \
			DisplaySource.cpp                                      \
			SourceInterface.cpp                                    \
			QueueBufferInterface.cpp                               \
			Output.cpp                                             \
			PixelStreamInterface.cpp                               \
			MetaDataQueue.cpp)

# Classes common to all ST SoCs regardless of the display architecture
STM_COMMON := $(addprefix $(SRC_TOPDIR)/display/ip/,                           \
			stmmasteroutput.cpp                                    \
			stmdenc.cpp                                            \
			stmteletext.cpp)

# Classes required for all SoCs containing Gamma based hardware video
# composition
STM_GAMMA := $(addprefix $(SRC_TOPDIR)/display/gamma/,                         \
			GammaMixer.cpp)

ifeq ($(CONFIG_STM_FMDSW),y)
FMDSW_SRC := $(SRC_TOPDIR)/display/ip/fmdsw.cpp
endif

STM_SRC_FILES := $(CORESOURCEFILES) $(FMDSW_SRC) $(GENINITSRCS) $(STM_COMMON) $(STM_GAMMA)


# Pull in all the new IP objects. Eventually a lot of the above will disappear
# into here as well.
include $(STG_TOPDIR)/display/ip/ip.mak

ifneq ($(CONFIG_STG_OPTLEVEL),)
EXTRA_CXXFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif

ifeq ($(CONFIG_USE_SLAVED_VTG_INTERRUPTS),y)
EXTRA_CXXFLAGS += -DUSE_SLAVED_VTG_INTERRUPTS
EXTRA_CFLAGS += -DUSE_SLAVED_VTG_INTERRUPTS
endif

ifeq ($(CONFIG_USE_SAS_VTG_AS_MASTER),y)
EXTRA_CXXFLAGS += -DUSE_SAS_VTG_AS_MASTER
EXTRA_CFLAGS += -DUSE_SLAVED_VTG_INTERRUPTS
endif

EXTRA_CFLAGS += -Werror

ifneq ($(CONFIG_DO_NOT_USE_HQVDP),y)
EXTRA_CFLAGS += -DUSE_HQVDP
endif

ifeq (yy, $(CONFIG_SYSFS)$(SDK2_ENABLE_DISPLAY_ATTRIBUTES))
EXTRA_CFLAGS += -DSDK2_ENABLE_DISPLAY_ATTRIBUTES
endif

BUILD_SYSTEM_INFO = $(shell /bin/uname -a)
BUILD_USER = $(shell /usr/bin/whoami)
BUILD_DATE = $(shell /bin/date --iso-8601)

STMFB_ORIGINAL_SOURCE_PATH ?= $(shell dirname `readlink -e $(M)/../../Makefile`)

# extract and provide version information from git if available
COREDISPLAY_VERSION := $(shell cd $(STMFB_ORIGINAL_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)
# If our git-describe is unsuccessful, we should fall back to an RPMBuild generated file
ifeq ($(COREDISPLAY_VERSION),)
COREDISPLAY_VERSION := $(shell cat $(STG_TOPDIR)/sdk2pkg/product_version)
endif

# Add build information defines for just the coredisplay object which will
# appear in sysfs. Because of the time information this file will rebuild
# every time.
CFLAGS_coredisplay.o := -DKBUILD_SYSTEM_INFO="KBUILD_STR($(BUILD_SYSTEM_INFO))"     \
                        -DKBUILD_USER="KBUILD_STR($(BUILD_USER))"                   \
                        -DKBUILD_SOURCE="KBUILD_STR($(STMFB_ORIGINAL_SOURCE_PATH))" \
                        -DKBUILD_VERSION="KBUILD_STR($(COREDISPLAY_VERSION))"       \
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

