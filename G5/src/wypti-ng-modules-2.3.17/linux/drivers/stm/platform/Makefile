EXTRA_CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/dvb/frontend -Idrivers/media/dvb/frontends/ -Idrivers/media/dvb/dvb-core -I./
EXTRA_CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/dvb/frontends/ -I$(TREE_ROOT)/linux/include
EXTRA_CFLAGS+=-I$(ROOT)/usr/include

frontend-platform-objs:= platform.o

obj-m += frontend-platform.o

ifneq ($(FRONTEND_SWTS_ONLY),)
EXTRA_CFLAGS += -DSWTS_ONLY
endif
