EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/stm_registry/include/

SDK2_REGISTRY_ENABLE_ATTRIBUTES ?= y

ifeq (y, $(CONFIG_SYSFS))
ifeq (y, $(filter y, $(SDK2_ENABLE_ATTRIBUTES)))
ifeq (y, $(filter y, $(SDK2_REGISTRY_ENABLE_ATTRIBUTES)))
EXTRA_CFLAGS+=-DSDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES
REGISTRY_OBJS += registry_sysfs.o
endif
endif
endif

ifeq (y, $(CONFIG_DEBUG_FS))
ifeq (y, $(filter y, $(SDK2_ENABLE_ATTRIBUTES)))
ifeq (y, $(filter y, $(SDK2_REGISTRY_ENABLE_ATTRIBUTES)))
EXTRA_CFLAGS+=-DSDK2_REGISTRY_ENABLE_DEBUGFS_ATTRIBUTES
REGISTRY_OBJS += registry_debugfs.o
endif
endif
endif


obj-m           += stm_registry.o

REGISTRY_OBJS += registry_datatype.o  registry_internal.o  stm_registry.o registry_main.o registry_iterator.o registry_tree.o stm_registry_exports.o

RELATIVE_ROOT = ../../
stm_registry-objs := $(addprefix $(RELATIVE_ROOT)/stm_registry/src/, $(REGISTRY_OBJS))
