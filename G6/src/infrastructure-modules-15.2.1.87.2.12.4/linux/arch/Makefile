EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/
RELATIVE_ROOT = ../../

ARCH_OBJS := arch_main.o

board-y := boards
board-$(CONFIG_ARCH_STM) := stm

platform-$(CONFIG_MACH_STM_FLI7610) := fli7610
platform-$(CONFIG_SH_ST_HDK7108) := hdk7108
platform-$(CONFIG_MACH_STM_STIH407_VSOC) := stih407_vsoc
ifdef CONFIG_MACH_STM_B2000
platform-$(CONFIG_MACH_STM_STIH415) := b2000
platform-$(CONFIG_MACH_STM_STIH416) := b2000_h416
endif
ifdef CONFIG_MACH_STM_B2020
platform-$(CONFIG_MACH_STM_STIH415)  := b2020
platform-$(CONFIG_MACH_STM_STIH416)  := b2020_h416
endif
ifdef CONFIG_MACH_STM_B2105
platform-$(CONFIG_MACH_STM_STIH415) := b2105
platform-$(CONFIG_MACH_STM_STIH416) := b2105_h416
endif
ifdef CONFIG_MACH_STM_B2116
platform-$(CONFIG_MACH_STM_STIH416) := b2116
endif

platform-$(CONFIG_MACH_STM_B2092) := b2092

ifeq ($(ARCH),sh)
ARCH_DIR := $(ARCH)/$(board-y)/mach-$(platform-y)
ARCH_OBJS += $(ARCH_DIR)/infra_setup.o
else
ifeq ($(CONFIG_HCE_ARCH_ARM),y)
# VSOC - WA for HCE compilation support,
# ARCH in our case is armv7 and expected is arm
ARCH_DIR := arm/mach-$(board-y)
else
ARCH_DIR := $(ARCH)/mach-$(board-y)
endif
ARCH_OBJS += $(ARCH_DIR)/infra_board-$(platform-y).o
endif

ifneq ($(platform-y),)
# if platformm is specified
obj-m           += stm_arch.o
stm_arch-objs := $(addprefix $(RELATIVE_ROOT)/arch/, $(ARCH_OBJS))
endif
