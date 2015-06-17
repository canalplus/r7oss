EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include

RELATIVE_ROOT = ../../
obj-m   += sbc_test.o
SBC_TEST_OBJS := power_wakeup_reason.o

sbc_test-objs := $(addprefix $(RELATIVE_ROOT)/stm_power/test/src/, $(SBC_TEST_OBJS))
