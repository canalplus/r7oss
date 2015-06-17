ifeq ($(CONFIG_MACH_STM_STIH407),y)

include $(STG_TOPDIR)/linux/kernel/drivers/stm/hdmirx/hdmirx.mak

hdmirx-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
hdmirx-objs := $(patsubst %.c,%.o,$(hdmirx-objs))

obj-m += hdmirx-stiH407.o

hdmirx-stiH407-objs := $(hdmirx-objs)

endif
