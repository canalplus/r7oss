# STM Infrastructure include path
ccflags-y+=-I$(CONFIG_INFRA_PATH)/include

# STM CoreDisplay include path
ccflags-y+=-I$(CONFIG_STGFB_PATH)/include
ccflags-y+=-I$(CONFIG_STGFB_PATH)/linux/kernel/include

# Local tree include path
ccflags-y+=-I$(TREE_ROOT)/linux/
ccflags-y+=-I$(TREE_ROOT)/linux/stmedia/
ccflags-y+=-I$(TREE_ROOT)/linux/include/
ccflags-y+=-I$(TREE_ROOT)/linux/v4l2/common/

ccflags-y += -Werror

ifeq ($(CONFIG_MACH_STM_STIH416),y)
ccflags-y += -DCONFIG_MACH_STM_STIH416
endif

ifeq ($(CONFIG_VIDEO_V4L2),)
$(error STLinuxTV V4L2 driver require CONFIG_VIDEO_V4L2 support)
endif

ifeq ($(CONFIG_MEDIA_CONTROLLER),)
$(error STLinuxTV require Media Controller support)
endif

stmvout-objs := stmvout_buffers.o                             \
		stmvout_display.o                             \
		stmvout_ctrl.o                                \
		stmvout_driver.o

stmvbi-objs := vbiout/stmvbi_driver.o

obj-m += capture/

obj-m += stmvout.o
obj-m += stmvbi.o
#obj-m += hdmi/

obj-$(CONFIG_STLINUXTV_ENCODER) += encoder/
obj-$(CONFIG_STLINUXTV_TSMUX) += tsmux/

obj-$(CONFIG_STLINUXTV_HDMIRX) += hdmirx/
obj-$(CONFIG_STLINUXTV_VXI) += vxi/

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_C8JPG)))
obj-m += stm-c8jpg/
endif

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_V4L2_DECODER)))
 obj-m += decoder/
endif
