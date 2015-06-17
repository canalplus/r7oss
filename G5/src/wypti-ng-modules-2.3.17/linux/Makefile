EXTRA_CFLAGS+=-I$(CONFIG_KERNEL_BUILD)/include2 \
	-I$(CONFIG_KERNEL_BUILD)/include \
	-include $(CONFIG_KERNEL_BUILD)/include/linux/autoconf.h \
	-I$(CONFIG_KERNEL_PATH)/include -D__KERNEL__

EXTRA_CFLAGS+=-I$(TREE_ROOT)/linux/include/
EXTRA_CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/dvb/frontends/
EXTRA_CFLAGS+=-I$(ROOT)/usr/include/linux


EXTRA_CFLAGS += -D__SH4__

obj-y += drivers/media/dvb/stm/frontend/
obj-y += drivers/media/dvb/frontends/
obj-y += drivers/stm/platform/
