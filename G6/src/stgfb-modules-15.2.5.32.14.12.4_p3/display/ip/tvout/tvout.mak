# Classes for the TVOut IP block.

# Currently always build the VIP and Pre-formatter blocks.
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/tvout/,                 \
                   stmpreformatter.cpp                                        \
                   stmvip.cpp)

ifneq ($(CONFIG_TVOUT_HD),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/tvout/,                 \
                   stmmaintvoutput.cpp)

endif

ifneq ($(CONFIG_TVOUT_SD),)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/tvout/,                 \
                   stmauxtvoutput.cpp)

endif

ifneq ($(CONFIG_TVOUT_DENC),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/tvout/, stmtvoutdenc.cpp)
endif

ifeq ($(CONFIG_TVOUT_TELETEXT),y)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/tvout/, stmtvoutteletext.cpp)
EXTRA_CXXFLAGS += -DCONFIG_TVOUT_TELETEXT=1
else
EXTRA_CXXFLAGS += -DCONFIG_TVOUT_TELETEXT=0
endif

ifneq ($(CONFIG_FDVO),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/tvout/,                  \
                   stmc8fdvo_v2_1.cpp                                          \
                   stmvipdvo.cpp)
endif


