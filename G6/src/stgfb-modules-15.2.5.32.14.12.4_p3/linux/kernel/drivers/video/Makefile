ifeq ($(CONFIG_FB),)
$(warning Framebuffer not configured in kernel, skipping stmfb module)
else
stmfb-objs :=	stmfbops.o \
		stmfbioctl.o \
		stmfbvar.o \
		stmfboutconfig.o \
		stmfbplatform.o

ifeq (y, $(CONFIG_SYSFS))
ifeq (y, $(filter y, $(SDK2_ENABLE_ATTRIBUTES)$(SDK2_ENABLE_STMFB_ATTRIBUTES)))
EXTRA_CFLAGS += -DSDK2_ENABLE_STMFB_ATTRIBUTES
stmfb-objs +=	stmfbsysfs.o
endif
endif

EXTRA_CFLAGS += -Werror

obj-m += stmfb.o

endif
