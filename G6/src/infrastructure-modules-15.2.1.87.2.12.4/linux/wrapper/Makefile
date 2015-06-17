EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include/

obj-m           += stm_wrapper.o

INFRA_OBJS := infra_os_wrapper.o \
		   infra_queue.o \
		   infra_exports.o \
		   infra_proc_interface.o \
		   infra_proc.o \
		   infra_main.o

RELATIVE_ROOT = ../../
stm_wrapper-objs := $(addprefix $(RELATIVE_ROOT)/stm_wrapper/src/, $(INFRA_OBJS))
