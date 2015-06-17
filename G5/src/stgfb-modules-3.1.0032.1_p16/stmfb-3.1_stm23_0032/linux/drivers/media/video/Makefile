ifeq ($(CONFIG_VIDEO_DEV),)
$(warning V4L2 not configured in kernel, skipping V4L2 modules)
else
stm_v4l2-objs := $(SRC_TOPDIR)/linux/drivers/media/video/stm_v4l2.o

stmvout-objs := $(addprefix $(SRC_TOPDIR)/linux/drivers/media/video/, \
			stmvout_buffers.o                             \
			stmvout_display.o                             \
			stmvout_ctrl.o                                \
			stmvout_driver.o)

stmvout-objs := $(stmvout-objs)

stmvbi-objs := $(SRC_TOPDIR)/linux/drivers/media/video/stmvbi_driver.o

obj-m += stm_v4l2.o
obj-m += stmvout.o
obj-m += stmvbi.o
endif