# Classes for display timing control, including frequency synthesizers and
# video timing generators.

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/displaytiming/,         \
                   stmvtg.cpp                                                 \
                   stmfsynth.cpp)

ifneq ($(CONFIG_C8VTG),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/displaytiming/,         \
                        stmc8vtg.cpp                                          \
                        stmc8vtg_v8_4.cpp)
endif

ifneq ($(CONFIG_CLOCK_LLA),)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/displaytiming/,         \
                        stmfsynthlla.cpp                                      \
                        stmclocklla.cpp)

else # CONFIG_CLOCK_LLA

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/displaytiming/,         \
                   stmc32_4fs660.cpp                                          \
                   stmc65_4fs216.cpp)
ifneq ($(CONFIG_FLEXCLKGEN),)
STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/displaytiming/,         \
                        stmflexclkgen_fs660.cpp)
endif

endif # CONFIG_CLOCK_LLA
