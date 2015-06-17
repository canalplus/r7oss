include $(STG_TOPDIR)/linux/kernel/drivers/stm/hdmi/hdmi.mak
EXTRA_CFLAGS += -I$(STG_TOPDIR)/linux/kernel/drivers/stm/hdmi
hdmirepeater-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
hdmirepeater-objs := $(patsubst %.c,%.o,$(hdmirepeater-objs))

obj-m += hdmirepeater.o
hdmirepeater-objs := $(hdmirepeater-objs) hdmirepkthread.o hdmirepdev.o


