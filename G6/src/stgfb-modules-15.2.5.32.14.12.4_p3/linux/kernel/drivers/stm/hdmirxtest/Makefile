ifeq (y, $(filter y, $(CONFIG_MACH_STM_STIH416) $(CONFIG_MACH_STM_STIH407)))
HDMIRX_KERNEL_MODULE_TEST :=y
include $(STG_TOPDIR)/hdmirx/hdmirx.mak

ifeq ($(CONFIG_MACH_STM_STIH416),y)
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_PIXSTREAM
HDMIRX_PIXSTREAM_TEST :=y
endif
endif

ifeq ($(HDMIRX_KERNEL_MODULE_TEST),y)

EXTRA_CFLAGS += \
   $(CFLAGS_HDRX) \
   -I$(INFRA_PATH)/include \
   -I$(STMFB_PATH)/include \
   -I$(KERNEL_ROOT)/include/linux/stm

obj-m += hdmirxtest.o

EXTRA_CFLAGS += -Werror

hdmirxtest-objs := audio_enable_test.o \
                   get_property_test.o \
                   hdmirx_test.o \
                   hot_plug_test.o \
                   mode_detection_test.o \
                   open_close_test.o \
                   read_edid_test.o \
                   update_edid_test.o

ifeq ($(HDMIRX_PIXSTREAM_TEST),y)
hdmirxtest-objs += source_pixelstream_test.o
endif

endif
