ifeq ($(CONFIG_MACH_STM_STIH407),y)

#$(info Building Pixel Capture for STiH407 !!)

ifeq ($(CONFIG_ARM),y)
KBUILD_CPPFLAGS += -DCONFIG_STM_HOST_CPU
endif

include $(STG_TOPDIR)/linux/kernel/drivers/stm/pixel_capture/capture.mak

include $(STG_TOPDIR)/pixel_capture/soc/stiH407/soc.mak

stiH407-capture-objs := $(patsubst %.cpp,%.o,$(STM_CAPTURE_SRC_FILES))
stiH407-capture-objs := $(patsubst %.c,%.o,$(stiH407-capture-objs))

obj-m += stm-pixel-capture-stiH407.o

stm-pixel-capture-stiH407-objs := $(stiH407-capture-objs) stiH407.o

endif
