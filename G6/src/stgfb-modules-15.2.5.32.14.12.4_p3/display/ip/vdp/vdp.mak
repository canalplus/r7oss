# Classes required for SoCs containing a VDP display
ifneq ($(CONFIG_VDP),)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/vdp/,  \
			VdpFilter.cpp                        \
			VdpPlane.cpp)

endif
