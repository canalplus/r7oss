EXTRA_CFLAGS += -I$(STG_TOPDIR)/vxi/generic

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/vxi/generic/, \
  VxiDevice.cpp                                        \
  vxi.cpp)
