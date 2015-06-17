ifneq ($(CONFIG_GDP),)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/gdp/,   \
                        GdpPlane.cpp                          \
                        VBIPlane.cpp)

endif
