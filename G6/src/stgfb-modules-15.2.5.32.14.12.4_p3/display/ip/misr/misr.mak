# Classes for the MISR (IP block name:SPIRIT) Lib:c8tvo_sasg1_hdtvout
# configuration.

ifneq ($(CONFIG_MISR_TVOUT),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/misr/,         \
                   stmmisrtvout.cpp                                  \
                   stmmisrmaintvout.cpp                              \
                   stmmisrauxtvout.cpp)
endif
