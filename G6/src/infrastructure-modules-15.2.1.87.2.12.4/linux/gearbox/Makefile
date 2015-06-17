EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_gearbox/include/

obj-m           += stm_gearbox.o

GEARBOX_OBJS := stm_gearbox_core.o

RELATIVE_ROOT = ../../
stm_gearbox-objs := $(addprefix $(RELATIVE_ROOT)/stm_gearbox/src/, $(GEARBOX_OBJS))
