ifeq (y, $(filter y, $(CONFIG_MACH_STM_STIH416) $(CONFIG_MACH_STM_STIH407)))

EXTRA_CFLAGS+=-I$(STG_TOPDIR)/include
EXTRA_CFLAGS+=-I$(STG_TOPDIR)/private/include
EXTRA_CFLAGS+=-I$(STG_TOPDIR)/linux/kernel/include
EXTRA_CFLAGS+=-I$(STG_TOPDIR)/linux/kernel/drivers/video

ifeq (y, $(filter y, $(CONFIG_MACH_STM_STIH407)))
ccflags-y += -DHDMIRX_MODE_DETECTION_TEST
EXTRA_CFLAGS += -DHDMIRX_MODE_DETECTION_TEST
include $(STG_TOPDIR)/hdmirx/hdmirx.mak

EXTRA_CFLAGS += \
   $(CFLAGS_HDRX) \
   -I$(INFRA_PATH)/include \
   -I$(STMFB_PATH)/include \
   -I$(KERNEL_ROOT)/include/linux/stm
endif

obj-m += pixelcapturetest.o

EXTRA_CFLAGS += -Werror

pixelcapturetest-objs :=	capture_test.o \
					common.o \
					display.o \
					main_to_aux.o

ifeq (y, $(filter y, $(CONFIG_MACH_STM_STIH407)))
pixelcapturetest-objs += hdmirx.o
endif

endif
