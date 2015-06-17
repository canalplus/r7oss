EXTRA_CFLAGS += -I$(STG_TOPDIR)/scaler/ip/fvdp

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/scaler/soc/stiH416/,        \
  stih416_scaler_device.cpp)
