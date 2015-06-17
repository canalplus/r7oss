EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_event/include

obj-m           += infra_test_common.o


INFRA_TEST_INTERFACE_OBJS :=   infra_test_interface.o \
				infra_token.o \



infra_test_common-objs := $(addprefix ../../stm_wrapper/test/, $(INFRA_TEST_INTERFACE_OBJS))
