EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/stm_ci/include/
EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/arch/include
EXTRA_CFLAGS+=-Werror -Wall

EXTRA_CFLAGS+=-I$(INFRASTRUCTURE)/include/
obj-m           += stm_ci.o

CI_OBJS := stm_ci_main.o \
		   stm_ci.o \
		   ci_utils.o \
		   ci_protocol.o \
		   ci_internal.o \
		   ci_handler.o \
		   ci_debugfs.o\
		   hal/ci_hal.o \
		   hal/ci_hal_pad.o \
		   hal/ci_hal_epld.o \
		   ci_user_interface.o \

RELATIVE_ROOT = ../../
stm_ci-objs := $(addprefix $(RELATIVE_ROOT)/stm_ci/src/, $(CI_OBJS))

