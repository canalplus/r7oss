ccflags-y+=-I$(CONFIG_INFRA_PATH)/include
ccflags-y+=-I$(CONFIG_STGFB_PATH)/include
ccflags-y+=-I$(CONFIG_PLAYER2_PATH)/player/include
ccflags-y+=-I$(TREE_ROOT)/linux/alsa/
ccflags-y+=-I$(TREE_ROOT)/linux/stmedia/
ccflags-y+=-I$(TREE_ROOT)/linux/include
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/capture
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/decoder/audio
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/decoder/common
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/hdmi
ccflags-y+=-Werror

#Either hdmirx->dvp or hdmirx->plane
ifeq (y, $(filter y, $(CONFIG_STLINUXTV_DVP)))
 ccflags-y+=-DCONFIG_STLINUXTV_DVP
else
 ifeq (y, $(filter y, $(CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS)))
   ccflags-y+=-DCONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
  else
   $(error define CONFIG_STLINUXTV_DVP or CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS from config.in)
 endif
endif

#Need to differentiate for audio subdev
ifeq (y, $(filter y, $(CONFIG_STLINUXTV_I2S_CAPTURE)))
 ccflags-y+=-DCONFIG_STLINUXTV_I2S_CAPTURE
else
 ifeq (y, $(filter y, $(CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS)))
  ccflags-y+=-DCONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
 else
  $(error define CONFIG_STLINUXTV_I2S_CAPTURE or CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS from config.in)
 endif
endif

stm_v4l2_hdmirx-objs := stm_extin_hdmirx.o
obj-m += stm_v4l2_hdmirx.o

