EXTRA_CFLAGS += -I$(STG_TOPDIR)/scaler/ip/blitter

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/scaler/soc/stiH407/,        \
  stih407_scaler_device.cpp)
