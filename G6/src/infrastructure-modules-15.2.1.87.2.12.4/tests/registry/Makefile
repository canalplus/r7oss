EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_registry/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_wrapper/include/

obj-m           += registry_test.o

REGISTRY_TEST_OBJS := registry_test_main.o registry_test_memoryleak.o registry_test_stress.o registry_test_functional.o registry_test_datatype.o

RELATIVE_ROOT = ../../
registry_test-objs := $(addprefix $(RELATIVE_ROOT)/stm_registry/test/src/, $(REGISTRY_TEST_OBJS))
