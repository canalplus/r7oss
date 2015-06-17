EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/include
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_api
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_hal/compat
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme/multiplexor
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme/wrapper
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme/infrastructure
EXTRA_CFLAGS += -I$(STM_TE_TOPDIR)/stm_te_tsmux_mme/mme
EXTRA_CFLAGS += -DST_OSLINUX

# Include paths from other STKPI components
ifneq "$(CONFIG_INFRASTRUCTURE)" ""
EXTRA_CFLAGS += -I$(CONFIG_INFRASTRUCTURE)/include
endif
ifneq "$(CONFIG_PLAYER2_PATH)" ""
EXTRA_CFLAGS += -I$(CONFIG_PLAYER2_PATH)/player/include/
endif

EXTRA_CFLAGS += -Werror

STM_TE_TSMUX_MME_OBJS += \
	multiplexor/multiplexor_private.o \
	multiplexor/multiplexor.o \
	wrapper/tsmux_mme.o \
	mme/emulation.o \
	te_tsmux_mme.o \
	infrastructure/cring.o

ifeq ($(CONFIG_CPU_SH4),y)
STM_TE_TSMUX_MME_OBJS += \
	infrastructure/divdi3.o \
	infrastructure/udivdi3.o \
	infrastructure/div64.o
else
STM_TE_TSMUX_MME_OBJS += \
	infrastructure/aeabi_ldivmod.o
endif

obj-m += stm_te_tsmux_mme.o

RELATIVE_ROOT = ../..
stm_te_tsmux_mme-objs := $(addprefix $(RELATIVE_ROOT)/stm_te_tsmux_mme/, $(STM_TE_TSMUX_MME_OBJS))
