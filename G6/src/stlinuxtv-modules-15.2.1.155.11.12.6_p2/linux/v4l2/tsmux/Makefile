# STM Infrastructure include path
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include

# STM Streaming-Engine include path
ccflags-y+=-I$(CONFIG_PLAYER2_PATH)/player/standards/
ccflags-y+=-I$(CONFIG_PLAYER2_PATH)/player/include/

# STM Transport-engine include path
ccflags-y+=-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include

# STM CoreDisplay include path
ccflags-y += -I$(CONFIG_STGFB_PATH)
ccflags-y += -I$(CONFIG_STGFB_PATH)/include

# STM Kernel include path
ifeq "$(shell [ $(VERSION) -ge 3 -a $(PATCHLEVEL) -ge 7 ] && echo true)" "true"
 ccflags-y+=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb/dvb-core/
else
 ccflags-y +=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb-core/
endif

ccflags-y += -Werror

# STLinuxTV headers
ccflags-y+=-I$(TREE_ROOT)/linux/include/
ccflags-y+=-I$(TREE_ROOT)/linux/stmedia/
ccflags-y+=-I$(TREE_ROOT)/linux/player2_dvb/
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/encoder
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/tsmux

obj-m += stm_v4l2_tsmuxer.o
stm_v4l2_tsmuxer-objs += stm_v4l2_tsmux.o
