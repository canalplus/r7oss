EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_event/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include
RELATIVE_ROOT = ../../

obj-m           += evt_test.o

EVENT_TEST_OBJS := evt_test_main.o \
		   evt_test_signaler.o \
		   evt_test_sub_1.o \
		   evt_test_sub_2.o \
		   evt_test_sub_3.o \
		   evt_test_subscriber.o

evt_test-objs := $(addprefix $(RELATIVE_ROOT)/stm_event/test/src/, $(EVENT_TEST_OBJS))
