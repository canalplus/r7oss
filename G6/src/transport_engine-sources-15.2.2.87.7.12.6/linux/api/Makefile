EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/include
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_api
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal/compat
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme/wrapper
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme/mme

$(warning CFlags are $(EXTRA_CFLAGS))

# Include paths from other STKPI components
ifneq "$(CONFIG_INFRASTRUCTURE)" ""
EXTRA_CFLAGS += -I$(CONFIG_INFRASTRUCTURE)/include
endif
ifneq "$(CONFIG_PLAYER2_PATH)" ""
EXTRA_CFLAGS += -I$(CONFIG_PLAYER2_PATH)/player/include
EXTRA_CFLAGS += -I$(CONFIG_PLAYER2_PATH)/linux/drivers/stm/st_relay
endif

EXTRA_CFLAGS += -Werror -Wall

# Extract and Provide Version Information
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
TE_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)
# We only define TE_VERSION to the source tree if we were successful from git-describe
ifneq ($(TE_VERSION),)
CFLAGS_stm_te_main.o += -DTE_VERSION="KBUILD_STR($(TE_VERSION))"
endif

STM_TE_API_OBJS += stm_te.o \
	stm_te_main.o \
	te_object.o \
	te_global.o \
	te_demux.o \
	te_tsmux.o \
	te_input_filter.o \
	te_output_filter.o \
	te_pcr_filter.o \
	te_ts_index_filter.o \
	te_section_filter.o \
	te_pid_filter.o \
	te_rep_filter.o \
	te_ins_filter.o \
	te_ts_filter.o \
	te_pes_filter.o \
	te_tsg_filter.o \
	te_time.o \
	te_hal_obj.o \
	te_sysfs.o \
	te_buffer_work.o \
	te_tsg_sec_filter.o \
	te_tsg_index_filter.o

obj-m += stm_te_api.o

RELATIVE_ROOT = ../..
stm_te_api-objs := $(addprefix $(RELATIVE_ROOT)/stm_te_api/, $(STM_TE_API_OBJS))

