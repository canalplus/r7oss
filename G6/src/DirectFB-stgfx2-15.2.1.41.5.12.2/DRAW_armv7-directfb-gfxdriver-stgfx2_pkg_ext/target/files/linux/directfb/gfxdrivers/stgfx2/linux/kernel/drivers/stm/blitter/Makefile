BUILD_SYSTEM_INFO := $(shell uname -a)
BUILD_USER := $(shell whoami)
BUILD_DATE := $(shell /bin/date --iso-8601)
BUILD_SOURCE_PATH := $(shell readlink -e $(lastword $(MAKEFILE_LIST)) | sed -e 's,/linux/kernel/drivers/stm/blitter/Makefile,,g')
# if building from a git tree then use the git version, otherwise the version of the RPM
BUILD_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --tags 2>/dev/null)
ifeq ($(BUILD_VERSION),)
BUILD_VERSION := @STM_BLITTER_RPM_VERSION@
endif

ccflags-y := -Wswitch-enum
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wframe-larger-than=2080)
ifeq ($(CONFIG_STM_BLITTER_UNIFIED_DRIVER_DEVICE),y)
ccflags-y += -DUNIFIED_DRIVER
endif
#ccflags-y += -std=gnu99

ccflags-$(CONFIG_STM_BLITTER_DEBUG) += -O0

# Add build information definition for the stm-blitter object which will
# appear in debugfs. Because of the time information this file will rebuild
# every time.
CFLAGS_bdispII_debugfs.o += -DKBUILD_SYSTEM_INFO="KBUILD_STR($(BUILD_SYSTEM_INFO))" \
		  -DKBUILD_USER="KBUILD_STR($(BUILD_USER))"               \
		  -DKBUILD_SOURCE="KBUILD_STR($(BUILD_SOURCE_PATH))"      \
		  -DKBUILD_VERSION="KBUILD_STR($(BUILD_VERSION))"         \
		  -DKBUILD_DATE="KBUILD_STR($(BUILD_DATE))"

ifeq ($(CONFIG_STM_BLITTER_DEBUG),y)
# some files don't like -O0
CFLAGS_blitter.o += -O1
CFLAGS_class.o += -O1
CFLAGS_strings.o += -O1
CFLAGS_surface.o += -O1
endif
ifeq ($(CONFIG_STM_BLITTER_BDISP2_DEBUG),y)
CFLAGS_bdispII.o += -O1
CFLAGS_bdispII_aq.o += -O1
CFLAGS_bdispII_aq_operations.o += -O1
CFLAGS_bdispII_aq_state.o += -O1
CFLAGS_bdispII_debug.o += -O1
CFLAGS_bdispII_debugfs.o += -O1
CFLAGS_bdispII_device_features.o += -O1
CFLAGS_bdispII_fops.o += -O1
CFLAGS_bdispII_glue.o += -O1
CFLAGS_bdispII_nodes.o += -O1
CFLAGS_bdisp2_linuxkernel_glue.o += -O1
CFLAGS_device_stih415.o += -O1
CFLAGS_device_stih416.o += -O1
CFLAGS_device_stih407.o += -O1
endif

ifneq ($(CONFIG_OF),y)
ifeq ($(CONFIG_STM_BLITTER_UNIFIED_DRIVER_DEVICE),y)
# SoCs
device-$(CONFIG_CPU_SUBTYPE_FLI7510) += device/device_fli7510.o
device-$(CONFIG_CPU_SUBTYPE_FLI7540) += device/device_fli7540.o
device-$(CONFIG_CPU_SUBTYPE_STX5206) += device/device_stx5206.o
device-$(CONFIG_CPU_SUBTYPE_STX7100) += device/device_stx7109.o
device-$(CONFIG_CPU_SUBTYPE_STX7105) += device/device_stx7105.o device/device_stx7106.o
device-$(CONFIG_CPU_SUBTYPE_STX7108) += device/device_stx7108.o
device-$(CONFIG_CPU_SUBTYPE_STX7111) += device/device_stx7111.o
device-$(CONFIG_CPU_SUBTYPE_STX7141) += device/device_stx7141.o
device-$(CONFIG_CPU_SUBTYPE_STX7200) += device/device_stx7200.o
device-$(CONFIG_CPU_SUBTYPE_STXH205) += device/device_stih205.o
# either ST40 or ARM
device-$(CONFIG_CPU_SUBTYPE_FLI7610) += device/device_fli7610.o
device-$(CONFIG_MACH_STM_FLI7610) += device/device_fli7610.o
device-$(CONFIG_CPU_SUBTYPE_STIH407) += device/device_stih407.o
device-$(CONFIG_MACH_STM_STIH407) += device/device_stih407.o
device-$(CONFIG_CPU_SUBTYPE_STIH415) += device/device_stih415.o
device-$(CONFIG_MACH_STM_STIH415) += device/device_stih415.o
device-$(CONFIG_CPU_SUBTYPE_STIH416) += device/device_stih416.o
device-$(CONFIG_MACH_STM_STIH416) += device/device_stih416.o
else
device-$(CONFIG_ARM) += device/device_fli7610.o
device-$(CONFIG_ARM) += device/device_stih407.o
device-$(CONFIG_ARM) += device/device_stih415.o
device-$(CONFIG_ARM) += device/device_stih416.o
endif
endif

# BDispII
ccflags-$(CONFIG_STM_BLITTER_BDISP2) += -DBDISP2_IMPLEMENT_WAITSERIAL
CFLAGS_bdispII.o += -DKBUILD_VERSION="KBUILD_STR($(BUILD_VERSION))"
CFLAGS_bdispII_aq_state.o += -UBDISP2_DUMP_CHECK_STATE
CFLAGS_bdispII_aq_state.o += -DBDISP2_DUMP_CHECK_STATE_FAILED
CFLAGS_bdispII_aq_state.o += -UBDISP2_DUMP_SET_STATE

stm-bdispII-$(CONFIG_STM_BLITTER_BDISP2) := $(addprefix bdisp2/, \
	bdispII_aq_features.o \
	bdispII_aq_state.o bdispII_aq_operations.o \
	bdispII_device_features.o \
	bdispII_nodes.o \
	\
	bdisp2_linuxkernel_glue.o \
	bdispII.o bdispII_aq.o \
	bdispII_glue.o \
	\
	bdispII_fops.o \
	\
	bdispII_debugfs.o \
	)

ifeq ($(CONFIG_OF),y)
	stm-bdispII-$(CONFIG_STM_BLITTER_BDISP2) += device/device_dt.o
ifeq ($(CONFIG_STM_BLITTER_BDISP2_DEBUG),y)
	CFLAGS_device_dt.o += -O1
endif
endif

stm-bdispII-$(CONFIG_STM_BLITTER_BDISP2_DEBUG) += bdisp2/bdispII_debug.o
stm-bdispII-objs := $(stm-bdispII-y) $(stm-bdispII-m)

stm-blitter-$(CONFIG_STM_BLITTER_BDISP2) += $(stm-bdispII-objs)


# core stm blitter
ccflags-y += $(ccflags-m)

stm-blitter-$(CONFIG_STM_BLITTER) += blitter.o surface.o class.o
stm-blitter-$(CONFIG_STM_BLITTER) += strings.o

stm-blitter-objs := $(stm-blitter-y) $(stm-blitter-m)
ifeq ($(CONFIG_STM_BLITTER_UNIFIED_DRIVER_DEVICE),y)
stm-blitter-objs += $(device-y)
endif

obj-$(CONFIG_STM_BLITTER) += stm-blitter.o
ifneq ($(CONFIG_STM_BLITTER_UNIFIED_DRIVER_DEVICE),y)
obj-$(CONFIG_STM_BLITTER) += $(device-y)
endif
