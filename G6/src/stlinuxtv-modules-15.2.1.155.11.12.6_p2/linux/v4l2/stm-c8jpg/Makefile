#
# Makefile for stm-c8jpg
#

ccflags-y += -I$(TREE_ROOT)/linux/v4l2/
ccflags-y += -I$(TREE_ROOT)/linux/include

ccflags-y += -Werror

ifeq (y, $(filter y, $(CONFIG_VIDEO_STM_C8JPG_DEBUG)))
ccflags-y += -DCONFIG_VIDEO_STM_C8JPG_DEBUG
endif

ifneq ($(CONFIG_OF),y)
stm-c8jpg-$(CONFIG_MACH_STM_STIH416) += c8jpg-stih416.o
stm-c8jpg-$(CONFIG_MACH_STM_STIH315) += c8jpg-stih416.o
stm-c8jpg-$(CONFIG_MACH_STM_STIH305) += c8jpg-stih407.o
stm-c8jpg-$(CONFIG_MACH_STM_STIH407) += c8jpg-stih407.o
stm-c8jpg-$(CONFIG_MACH_STM_STIH310) += c8jpg-stih407.o
endif

stm-c8jpg-objs := c8jpg-v4l2.o \
		  c8jpg-hw.o \
		  c8jpg-drv.o \
		  $(stm-c8jpg-y)

obj-m += stm-c8jpg.o
