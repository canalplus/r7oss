EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_event/include/

SDK2_EVENT_ENABLE_STATISTICS ?= y

ifeq (y, $(CONFIG_DEBUG_FS))
ifeq (y, $(filter y, $(SDK2_ENABLE_STATISTICS)))
ifeq (y, $(filter y, $(SDK2_EVENT_ENABLE_STATISTICS)))
EXTRA_CFLAGS+=-DSDK2_EVENT_ENABLE_DEBUGFS_STATISTICS
EVENT_OBJS := event_debugfs.o
endif
endif
endif

obj-m           += stm_event.o

EVENT_OBJS += event_signaler.o \
		   event_subscriber.o \
		   stm_event_exports.o \
		   stm_event_main.o \
		   event_utils.o \
		   evt_signal_async.o \
		   stm_event.o

RELATIVE_ROOT = ../../
stm_event-objs := $(addprefix $(RELATIVE_ROOT)/stm_event/src/, $(EVENT_OBJS))
