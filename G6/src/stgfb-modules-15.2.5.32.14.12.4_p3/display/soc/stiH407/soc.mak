# Classes for STiH407 device
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/soc/stiH407/,               \
                        stiH407hdmi.cpp                                        \
                        stiH407hdmiphy.cpp                                     \
                        stiH407mainoutput.cpp                                  \
                        stiH407auxoutput.cpp                                   \
                        stiH407denc.cpp                                        \
                        stiH407mixer.cpp                                       \
                        stiH407device.cpp)
ifneq ($(CONFIG_FDVO),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/soc/stiH407/, stiH407dvo.cpp)
endif

