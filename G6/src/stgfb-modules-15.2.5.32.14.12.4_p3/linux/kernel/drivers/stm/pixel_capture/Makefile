
ifeq ($(CONFIG_ARM),y)
KBUILD_CPPFLAGS += -DCONFIG_STM_HOST_CPU
endif

include $(STG_TOPDIR)/linux/kernel/drivers/stm/pixel_capture/capture.mak

stm-capture-objs := $(patsubst %.cpp,%.o,$(STM_CAPTURE_SRC_FILES))
stm-capture-objs := $(patsubst %.c,%.o,$(stm-capture-objs))

obj-m += stm-pixel-capture.o

# Add soc specific file for compilation, to be removed when completely moving to DT
stm-pixel-capture-objs := $(stm-capture-objs)
