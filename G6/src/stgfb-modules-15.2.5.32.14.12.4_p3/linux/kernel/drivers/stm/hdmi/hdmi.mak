STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)../, \
  hdmiioctl.c \
  hdmiedid.c)

ifeq (y, $(CONFIG_SYSFS))
ifeq (y, $(filter y, $(SDK2_ENABLE_ATTRIBUTES)$(SDK2_ENABLE_HDMI_TX_ATTRIBUTES)))
EXTRA_CFLAGS += -DSDK2_ENABLE_HDMI_TX_ATTRIBUTES
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)../, hdmisysfs.o)
endif
endif
