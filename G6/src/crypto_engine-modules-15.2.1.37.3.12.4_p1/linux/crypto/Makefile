# crypto_adaptation makefile
# stm_ce adaptation layer for Linux kernel crypto api

ccflags-y += -Wall -Werror

# Include stm_ce headers
ccflags-y += -I$(TREE_ROOT)/include

# Include stm_ce linux user headers
ccflags-y += -I$(TREE_ROOT)/linux/include

# Define SEC_PROFILE macro
ifneq "$(STM_CE_SEC_PROFILE)" ""
    ccflags-y += -DSTM_CE_SEC_PROFILE=$(STM_CE_SEC_PROFILE)
#ccflags-y += -DSTM_CE_SEC_PROFILE=CA3
endif

# Include paths for other stkpi components
ifneq "$(CONFIG_INFRASTRUCTURE_PATH)" ""
ccflags-y += -I$(CONFIG_INFRASTRUCTURE_PATH)/include
endif
# Include paths to firmware specific headers
ifneq "$(SDK2_SOURCE_CRYPTO_ENGINE_FW_API_HEADERS)" ""
ccflags-y += -I$(SDK2_SOURCE_CRYPTO_ENGINE_FW_API_HEADERS)
endif

ifeq ($(CONFIG_STM_VIRTUAL_PLATFORM),y)
# As VSoC uses another target directory,we point to original include directory to avoid useless copy (contains mme, transformers .h files)
ccflags-y += -I$(THD_USER_INCLUDE)/usr/include
else
ccflags-y += -I$(INSTALL_PATH)/usr/include
endif

crypto_adaptation-objs += crypto_adaptation_main.o \
	ce_crypto.o \
	stm_cipher.o

obj-m += crypto_adaptation.o
