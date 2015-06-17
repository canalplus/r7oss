FBSOURCEFILES := $(addprefix $(SRC_TOPDIR)/linux/drivers/video/, \
					stmfbops.c stmfbioctl.c stmfbsysfs.c \
					stmfbvar.c stmfboutconfig.c stmfb.c)
ifeq ($(CONFIG_FB),)
$(warning Framebuffer not configured in kernel, skipping stmfb module)
else
obj-m += stmfb.o

stmfb-objs := $(patsubst %.c,%.o,$(FBSOURCEFILES))
stmfb-stb7100-objs := $(stmfb-objs)
endif
