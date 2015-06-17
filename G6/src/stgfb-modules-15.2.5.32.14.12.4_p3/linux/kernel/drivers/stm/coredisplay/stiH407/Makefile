ifeq ($(CONFIG_MACH_STM_STIH407),y)

KBUILD_CPPFLAGS += -DCONFIG_STM_HOST_CPU -DSTM_VIBE_USE_THREADED_IRQ=1

CONFIG_CLOCK_LLA       := y
CONFIG_C8VTG           := y

CONFIG_HDMI_TX         := y
CONFIG_HDMI_V29_IFRAME := y
CONFIG_HDMITX3G4_PHY   := y
CONFIG_HDF_V5_0        := y
CONFIG_TVOUT_HD        := y
CONFIG_TVOUT_SD        := y
CONFIG_FDVO            := y
CONFIG_TVOUT_TELETEXT  ?= y
CONFIG_TVOUT_DENC      := y
CONFIG_MISR_TVOUT      := y
CONFIG_HQVDPLITE       := y
CONFIG_VDP             := y
CONFIG_GDP             := y
CONFIG_QUEUE_BUFFER    := y

include $(STG_TOPDIR)/linux/kernel/drivers/stm/coredisplay/coredisplay.mak

include $(STG_TOPDIR)/display/soc/stiH407/soc.mak

stiH407-display-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES) $(STM_HQVDP))
stiH407-display-objs := $(patsubst %.c,%.o,$(stiH407-display-objs))

obj-m += stmcore-display-stiH407.o

stmcore-display-stiH407-objs := $(stiH407-display-objs) stiH407.o

endif
