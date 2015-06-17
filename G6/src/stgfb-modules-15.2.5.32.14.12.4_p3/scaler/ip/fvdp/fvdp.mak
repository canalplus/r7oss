EXTRA_CFLAGS += -I$(STG_TOPDIR)/scaler/ip/fvdp

# STM Fvdp include path
EXTRA_CFLAGS += -I$(STG_TOPDIR)/fvdp/
EXTRA_CFLAGS += -DCONFIG_MPE42

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/scaler/ip/fvdp/, \
	fvdp_scaler.cpp                                         \
	fvdp_task_node.cpp                                      \
	fvdp_pool.cpp)

