INFRA_BUILD_CEC?=0

ifeq ($(CONFIG_MACH_STM_FLI7610),y)
EXTRA_CFLAGS+=-DCEC_DEVICE_TV
endif

EXTRA_CFLAGS+=-DINFRA_BUILD_CEC=$(INFRA_BUILD_CEC)

EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_cec/include/
EXTRA_CFLAGS += -I$(CONFIG_STGFB_PATH)/include
EXTRA_CFLAGS += -I$(CONFIG_STGFB_PATH)/linux/kernel/include
EXTRA_CFLAGS+=-I$(STMFB)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include
RELATIVE_ROOT = ../../

obj-m           += cec_test.o

CEC_TEST_OBJS :=   cec_test_main.o

cec_test-objs := $(addprefix $(RELATIVE_ROOT)/stm_cec/test/src/, $(CEC_TEST_OBJS))
