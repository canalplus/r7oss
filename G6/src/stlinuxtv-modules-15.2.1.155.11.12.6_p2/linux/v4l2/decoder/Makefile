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
 ccflags-y+=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb-core/
else
 ccflags-y +=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb/dvb-core/
endif

ccflags-y += -Werror

# STLinuxTV headers
ccflags-y+=-I$(TREE_ROOT)/linux/
ccflags-y+=-I$(TREE_ROOT)/linux/include/
ccflags-y+=-I$(TREE_ROOT)/linux/stmedia/
ccflags-y+=-I$(TREE_ROOT)/linux/player2_dvb/
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/encoder
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/common
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/decoder/common
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/decoder/video
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/decoder/audio

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_V4L2_DECODER)))
 stm_v4l2_decoder-objs := common/stm_v4l2_decoder.o \
                          video/stm_v4l2_video_decoder.o \
                          video/stm_v4l2_video_ctrls.o \
                          audio/stm_v4l2_audio_decoder.o \
                          audio/stm_v4l2_audio_ctrls.o

 obj-m += stm_v4l2_decoder.o
endif
