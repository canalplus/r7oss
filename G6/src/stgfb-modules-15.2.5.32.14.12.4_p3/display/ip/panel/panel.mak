# Classes for the PANEL IP block.
ifneq ($(CONFIG_PANEL),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/panel/,                 \
                   stmdptx.cpp                                                \
                   stmodp.cpp                                                 \
                   stmodpdtg.cpp                                              \
                   stmpaneloutput.cpp)
include $(STG_TOPDIR)/display/ip/panel/mpe41/mpe41.mak
endif
