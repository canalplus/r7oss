EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/include
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal/compat
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal/config
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal/tango
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal/tango_lite

ifdef CONFIG_OF
STM_TE_HAL_OBJS += config/pti_dt.o
endif

# VSOC FIXME - To be removed as soon as Model included in platform and memory map updated in config.xml file
# - ISS Model currently slow down the platform, so if not needed, enable CONFIG_STM_VIRTUAL_PLATFORM_DISABLE_TANGO
#   to disable model usage. In this case we must stub the sw/fw syncrhonisation.
# - Then update bigphysarea region to 'bigphysarea' as memory map is not yet fully inline (config.xml in vsoc platform)
ifdef CONFIG_STM_VIRTUAL_PLATFORM
EXTRA_CFLAGS += -DSTPTI_TPLOADER -DSTPTI_DEBUG_PRINT_BUFFER_SIZE=65536
EXTRA_CFLAGS += -DSTPTI_BPA2_SECURE_REGION=\"bigphysarea\"
EXTRA_CFLAGS += -DCONFIG_STM_VIRTUAL_PLATFORM_DISABLE_TANGO
else
EXTRA_CFLAGS += -DSTPTI_TPLOADER -DSTPTI_DEBUG_PRINT_BUFFER_SIZE=65536
endif

# Extract and Provide Version Information
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
TE_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)
# We only define TE_VERSION to the source tree if we were successful from git-describe
ifneq ($(TE_VERSION),)
CFLAGS_pti_demux_hal_main.o += -DTE_VERSION="KBUILD_STR($(TE_VERSION))"
CFLAGS_pti_driver.o += -DTE_VERSION="KBUILD_STR($(TE_VERSION))"
endif

EXTRA_CFLAGS += -Werror -Wall

STM_TE_HAL_OBJS += objman/pti_container.o \
	objman/pti_object.o \
	objman/pti_list.o \
	tango/pti_swinjector.o \
	tango/pti_signal.o \
	tango/pti_session.o \
	tango/pti_index.o \
	tango/pti_slot.o \
	tango/pti_pdevice.o \
	tango/pti_dbg.o \
	tango/pti_filter.o \
	tango/pti_vdevice.o \
	tango/pti_dataentry.o \
	tango/pti_isr.o \
	tango/pti_evt.o \
	tango/pti_mailbox.o \
	tango/pti_buffer.o \
	tango/pti_debug_print.o \
	debug/pti_dbgfs.o \
	stfe/pti_stfe.o \
	stfe/pti_stfe_v1.o \
	stfe/pti_stfe_v2.o \
	stfe/pti_tsinput.o \
	pti_debug.o \
	pti_osal.o \
	pti_driver.o \
	pti_demux_hal_main.o \
	pti_demux_hal_exports.o \
	tango_lite/pti_pdevice_lite.o \
	tango_lite/pti_vdevice_lite.o \
	tango_lite/pti_swinjector_lite.o \
	tango_lite/pti_signal_lite.o \
	tango_lite/pti_buffer_lite.o \
	tango_lite/pti_slot_lite.o \
	tango_lite/pti_dbg_lite.o \
	tango_lite/pti_debug_print_lite.o

obj-m += stm_te_hal.o

RELATIVE_ROOT = ../..
stm_te_hal-objs := $(addprefix $(RELATIVE_ROOT)/stm_te_hal/, $(STM_TE_HAL_OBJS))
