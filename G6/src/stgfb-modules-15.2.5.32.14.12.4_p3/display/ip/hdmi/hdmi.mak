# Classes for the HDMI frame formatter management, HDMI phy and infoframe
# configuration.

ifneq ($(CONFIG_HDMI_TX),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdmi/,         \
                   stmhdmi.cpp                                       \
                   stmiframemanager.cpp)

ifneq ($(CONFIG_HDMI_DMA_IFRAME),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdmi/,         \
                   stmdmaiframes.cpp)
endif

ifneq ($(CONFIG_HDMI_V29_IFRAME),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdmi/,         \
                   stmv29iframes.cpp)
endif

ifneq ($(CONFIG_HDMITX2G25_PHY),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdmi/,         \
                   stmhdmitx2g25_33_c65_phy.cpp)
endif

ifneq ($(CONFIG_HDMITX3G4_PHY),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdmi/,         \
                   stmhdmitx3g4_c28_phy.cpp)
endif

ifneq ($(CONFIG_HDMITX3G0_PHY),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hdmi/,         \
                   stmhdmitx3g0_c55_phy.cpp)
endif

endif

