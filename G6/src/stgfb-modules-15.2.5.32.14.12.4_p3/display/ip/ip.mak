# Assorted Classes various IP blocks and general IP management.

ifneq ($(CONFIG_VTAC),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/, stmvtac.cpp)
endif

ifneq ($(CONFIG_FDVO),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/, stmfdvo.cpp)
include $(STG_TOPDIR)/display/ip/sync/sync.mak
endif

include $(STG_TOPDIR)/display/ip/displaytiming/displaytiming.mak
include $(STG_TOPDIR)/display/ip/analogsync/analogsync.mak
include $(STG_TOPDIR)/display/ip/hdf/hdf.mak
include $(STG_TOPDIR)/display/ip/misr/misr.mak
include $(STG_TOPDIR)/display/ip/tvout/tvout.mak
include $(STG_TOPDIR)/display/ip/videoPlug/videoPlug.mak

ifeq ($(CONFIG_HQVDPLITE),y)
include $(STG_TOPDIR)/display/ip/hqvdplite/hqvdplite.mak
endif

ifeq ($(CONFIG_VDP),y)
include $(STG_TOPDIR)/display/ip/vdp/vdp.mak
endif

ifeq ($(CONFIG_GDP),y)
include $(STG_TOPDIR)/display/ip/gdp/gdp.mak
endif

ifeq ($(CONFIG_QUEUE_BUFFER),y)
include $(STG_TOPDIR)/display/ip/queuebufferinterface/QueueBuffer.mak
endif

include $(STG_TOPDIR)/display/ip/hdmi/hdmi.mak
include $(STG_TOPDIR)/display/ip/pixelstream/pixelstream.mak

ifneq ($(CONFIG_PANEL),)
include $(STG_TOPDIR)/display/ip/panel/panel.mak
endif
