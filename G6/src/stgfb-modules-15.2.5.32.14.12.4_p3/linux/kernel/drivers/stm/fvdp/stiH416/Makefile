#
# CONFIG_CPU_SUBTYPE_STXXXX is no more defined for all coming ST chips
# based on arm CPU. So redefine it here just to keep sh support inside the driver
#
ifeq ($(CONFIG_MACH_STM_STIH416),y)
CONFIG_CPU_SUBTYPE_STIH416 :=y
endif

ifeq ($(CONFIG_CPU_SUBTYPE_STIH416),y)

CONFIG_MPE42      := y
CONFIG_FVDP       := y
FVDP_IOCTL        := y

include $(STG_TOPDIR)/linux/kernel/drivers/stm/fvdp/fvdp_module.mak

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/fvdp/stiH416/, \
  stiH416_fvdp.c)

ifneq ($(CONFIG_STG_OPTLEVEL),)
# For C file including linux header, compilation is failing when changing option level
# so keep default option level for them.
CFLAGS_REMOVE_stiH416_fvdp.o += -O$(CONFIG_STG_OPTLEVEL)
endif

stiH416-fvdp-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
stiH416-fvdp-objs := $(patsubst %.c,%.o,$(stiH416-fvdp-objs))

obj-m += stmcore-fvdp-stiH416.o

stmcore-fvdp-stiH416-objs := $(stiH416-fvdp-objs)

endif
