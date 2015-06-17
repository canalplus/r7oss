#
# CONFIG_CPU_SUBTYPE_STXXXX is no more defined for all coming ST chips
# based on arm CPU. So redefine it here just to keep sh support inside the driver
#
EXTRA_CFLAGS += -I$(STG_TOPDIR)/hdmirx/soc
EXTRA_CFLAGS += -I$(STG_TOPDIR)/hdmirx/include

ifeq ($(CONFIG_MACH_STM_STIH407),y)

EXTRA_CFLAGS += -I$(STG_TOPDIR)/hdmirx/soc/stiH407

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/hdmirx/soc/stiH407/, \
  stiH407_hdmirx_drv.c)
endif

ifeq ($(CONFIG_MACH_STM_STIH416),y)
EXTRA_CFLAGS += -I$(STG_TOPDIR)/hdmirx/soc/stiH416

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/hdmirx/soc/stiH416/, \
  stiH416_hdmirx_drv.c \
  stiH416_vtac.c)

endif

