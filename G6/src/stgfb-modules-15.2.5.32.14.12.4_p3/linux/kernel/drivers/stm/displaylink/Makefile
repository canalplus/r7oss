obj-m += displaylink.o

EXTRA_CFLAGS += -Werror

displaylink-objs := link.o linkmanager.o linksysfs.o linkmodule.o linkhdcp_fops.o linkproxy_client.o

ifeq (y, $(CONFIG_SYSFS))
ifeq (y, $(filter y, $(SDK2_ENABLE_ATTRIBUTES)$(SDK2_ENABLE_HDMI_TX_ATTRIBUTE)))
EXTRA_CFLAGS += -DSDK2_ENABLE_HDMI_TX_ATTRIBUTE
displaylink-objs += linksysfs.o
endif
endif

ifeq ($(CONFIG_MACH_STM_STIH407),y)
KBUILD_CPPFLAGS +=-DSTM_VIBE_USE_THREADED_IRQ=1
endif
