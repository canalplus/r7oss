CFLAGS += -I$(TREE_ROOT)/linux/include/linux/stm/ 
CFLAGS += -I$(TREE_ROOT)/linux/drivers/media/video/stm/
CFLAGS += -I$(TREE_ROOT)/linux/drivers/sound/pseudocard
CFLAGS += -I$(TREE_ROOT)/linux/drivers/stm/mmelog/
CFLAGS += -I$(CONFIG_STGFB_PATH)/include 
CFLAGS += -I$(CONFIG_STGFB_PATH)/linux
CFLAGS += -I$(CONFIG_STGFB_PATH)/linux/drivers/media/video
CFLAGS += -I$(CONFIG_KERNEL_PATH)/drivers/media/dvb/dvb-core 
CFLAGS += -DDEBUG_DVB

stmdvb-objs:=   backend.o \
		dvb_audio.o \
		dvb_demux.o \
		dvb_dvr.o \
		dvb_module.o \
		dvb_video.o \
		dvb_ca.o \
		dvb_v4l2.o \
		dvb_v4l2_audout.o \
		dvb_avr.o \
		dvb_avr_audio.o \
		dvb_avr_video.o \
		dvb_capture_pipe.o 

obj-$(CONFIG_STM_UNIFY_STMDVB) += stmdvb.o
