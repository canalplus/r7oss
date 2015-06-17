include $(STG_TOPDIR)/linux/kernel/drivers/stm/hdmi/hdmi.mak
EXTRA_CFLAGS += -I$(STG_TOPDIR)/linux/kernel/drivers/stm/hdmi
hdmi-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
hdmi-objs := $(patsubst %.c,%.o,$(hdmi-objs))

obj-m += hdmi.o
hdmi-objs := $(hdmi-objs) hdmitxkthread.o hdmitxdev.o


