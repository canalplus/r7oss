EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_memsrcsink/include/

#install: modules_install
obj-m           += memsrcsink_tests.o

MEMSINK_TEST_OBJS := mss_test_debugfs.o \
	             mss_test_main.o \
		     mss_test_producer.o \
		     mss_test_consumer.o \
		     mss_test_debugfs_interface.o \
		     mss_test_user_app_interface.o \
		     mss_test_vmalloc_sink.o \
		     mss_test_vmalloc_src.o

RELATIVE_ROOT = ../../
memsrcsink_tests-objs := $(addprefix $(RELATIVE_ROOT)/stm_memsrcsink/test/src/, $(MEMSINK_TEST_OBJS))



