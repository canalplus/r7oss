# STM CoreDisplay include path
ccflags-y += -I$(CONFIG_STGFB_PATH)/include

# STM Streaming-Engine include path
ccflags-y += -I$(CONFIG_PLAYER2_PATH)/player/include/

# STM Infrastructure include path
ccflags-y += -I$(CONFIG_INFRA_PATH)/include

ifeq ($(CONFIG_STM_VIRTUAL_PLATFORM),y)
# As VSOC uses another target directory for module installation, we use another
# variable to point to original include directory (avoid useless copy as
# contains mme, transformers .h files)
ccflags-y += -I$(THD_USER_INCLUDE)/usr/include
else
ccflags-y += -I$(TARGET_DIR)/usr/include
endif

ccflags-y += -I$(SDK2_SOURCE_audio_apiheaders_ext)

ifeq ($(CONFIG_MEDIA_CONTROLLER),)
$(error STLinuxTV PseudoCard driver require MediaController support)
endif

ccflags-y+=-I$(TREE_ROOT)/linux/include/linux/stm
ccflags-y+=-I$(TREE_ROOT)/linux/stmedia

ccflags-y += -Werror

pseudocard-objs := pseudo_mixer.o pseudo_mc.o pseudo_hw.o pseudo_pcmproc_tuning.o

obj-m += pseudocard.o
