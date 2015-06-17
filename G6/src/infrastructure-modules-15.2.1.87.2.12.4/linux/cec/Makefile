EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_cec/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include

ifeq ($(INFRA_ERROR_TRACE_COMPILE),1)
export $(INFRA_ERROR_TRACE_COMPILE)
endif

ifeq ($(INFRA_ERROR_TRACE_RUNTIME),1)
export $(INFRA_ERROR_TRACE_RUNTIME)
endif

obj-m           += stm_cec.o

CEC_OBJS := cec_core.o \
		   cec_hal.o \
		   cec_main.o \
		   stm_cec.o

RELATIVE_ROOT = ../../
stm_cec-objs := $(addprefix $(RELATIVE_ROOT)/stm_cec/src/, $(CEC_OBJS))
