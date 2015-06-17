ifeq ($(CONFIG_MACH_STM_STIH416),y)

include $(STG_TOPDIR)/linux/kernel/drivers/stm/hdmirx/hdmirx.mak

hdmirx-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
hdmirx-objs := $(patsubst %.c,%.o,$(hdmirx-objs))

obj-m += hdmirx-stiH416.o

hdmirx-stiH416-objs := $(hdmirx-objs)

endif
