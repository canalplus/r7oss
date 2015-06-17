ccflags-y += -Wall -Werror

# stm_ce kernel module makefile
ccflags-y += -I$(TREE_ROOT)/include

# Include paths to other stkpi components
ifneq "$(CONFIG_INFRASTRUCTURE_PATH)" ""
ccflags-y += -I$(CONFIG_INFRASTRUCTURE_PATH)/include
endif
ifneq "$(CONFIG_TRANSPORT_ENGINE_PATH)" ""
ccflags-y += -I$(CONFIG_TRANSPORT_ENGINE_PATH)/include
endif
ifneq "$(CONFIG_MULTICOM_PATH)" ""
ccflags-y += -I$(CONFIG_MULTICOM_PATH)/source/include
endif
# Include paths to firmware specific headers
ifneq "$(SDK2_SOURCE_CRYPTO_ENGINE_FW_API_HEADERS)" ""
ccflags-y += -I$(SDK2_SOURCE_CRYPTO_ENGINE_FW_API_HEADERS)
endif

# Get version from source control (if available)
REAL_SOURCE_PATH := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
STM_CE_VERSION := $(shell cd $(REAL_SOURCE_PATH) && git describe --dirty --tags --long 2>/dev/null)
ifneq "$(STM_CE_VERSION)" ""
ccflags-y += -DSTM_CE_VERSION="KBUILD_STR($(STM_CE_VERSION))"
endif

# Enable CE MME comms on a per-platform basis when firmware is available
ifdef CONFIG_CPU_SUBTYPE_STX7108
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifdef CONFIG_CPU_SUBTYPE_STIH415
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifdef CONFIG_CPU_SUBTYPE_STIH416
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifdef CONFIG_MACH_STM_STIH305
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifdef CONFIG_MACH_STM_STIH310
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifdef CONFIG_MACH_STM_STIH407
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifdef CONFIG_MACH_STM_STIH410
ccflags-y += -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED
endif

ifeq ($(CONFIG_STM_VIRTUAL_PLATFORM),y)
# As VSoC uses another target directory,we point to original include directory to avoid useless copy (contains mme, transformers .h files)
ccflags-y += -I$(THD_USER_INCLUDE)/usr/include

# Disable CE firmware load for virtual platforms
ccflags-y := $(filter-out -DCONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED, $(ccflags-y))

else
ccflags-y += -I$(INSTALL_PATH)/usr/include
endif

# Allow overriding of default key rules
ifneq "$(STM_CE_SEC_PROFILE)" ""
    ccflags-y += -DSTM_CE_SEC_PROFILE=$(STM_CE_SEC_PROFILE)
else
    ccflags-y += -DSTM_CE_SEC_PROFILE=DEFAULT
endif

stm_ce-objs += stm_ce_pm.o \
	stm_ce_api.o \
	stm_ce_api_session.o \
	stm_ce_api_transform.o \
	ce_transform_m2m.o \
	ce_hal_manager.o \
	helpers.o \
	ce_path.o \
	ce_dt.o \
	ce_path_session.o \
	ce_path_transform.o \
	ce_path_mme.o

obj-m += stm_ce.o
