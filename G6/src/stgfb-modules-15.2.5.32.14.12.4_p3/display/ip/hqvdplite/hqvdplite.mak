# Classes required for SoCs containing a HQVDP Lite display
ifneq ($(CONFIG_HQVDPLITE),)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hqvdplite/,                   \
                        HqvdpLitePlane.cpp                                          \
                        stmiqidefaultcolor.cpp                                      \
                        stmiqicti.cpp                                               \
                        stmiqile.cpp                                                \
                        stmiqipeaking.cpp)



include $(STG_TOPDIR)/display/ip/hqvdplite/lld/lld.mak
endif

