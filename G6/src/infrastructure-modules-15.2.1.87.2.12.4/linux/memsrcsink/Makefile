EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_memsrcsink/include/

obj-m           += stm_memsrcsink.o

MEMSRCSINK_OBJS := memsrc.o \
		   memsink.o \
		   memutil.o

RELATIVE_ROOT = ../../
stm_memsrcsink-objs := $(addprefix $(RELATIVE_ROOT)/stm_memsrcsink/src/, $(MEMSRCSINK_OBJS))
