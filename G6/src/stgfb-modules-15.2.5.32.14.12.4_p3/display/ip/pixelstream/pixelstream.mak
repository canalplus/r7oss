# Classes for the PIXELSTREAM IP block.

ifneq ($(CONFIG_PIXELSTREAM),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/pixelstream/,             \
                   MpePixelStreamInterface.cpp)

endif
