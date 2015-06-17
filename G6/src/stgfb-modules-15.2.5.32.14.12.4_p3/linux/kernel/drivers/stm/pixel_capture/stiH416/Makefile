#
# CONFIG_CPU_SUBTYPE_STXXXX is no more defined for all coming ST chips
# based on arm CPU. So redefine it here just to keep sh support inside the driver
#
ifeq ($(CONFIG_MACH_STM_STIH416),y)
CONFIG_CPU_SUBTYPE_STIH416 :=y
endif

ifeq ($(CONFIG_CPU_SUBTYPE_STIH416),y)

#$(info Building Pixel Capture for STiH416 !!)

ifeq ($(CONFIG_ARM),y)
KBUILD_CPPFLAGS += -DCONFIG_STM_HOST_CPU
endif

include $(STG_TOPDIR)/linux/kernel/drivers/stm/pixel_capture/capture.mak

include $(STG_TOPDIR)/pixel_capture/soc/stiH416/soc.mak

stiH416-capture-objs := $(patsubst %.cpp,%.o,$(STM_CAPTURE_SRC_FILES))
stiH416-capture-objs := $(patsubst %.c,%.o,$(stiH416-capture-objs))

obj-m += stm-pixel-capture-stiH416.o

stm-pixel-capture-stiH416-objs := $(stiH416-capture-objs) stiH416.o

endif

