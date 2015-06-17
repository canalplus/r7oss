EXTRA_CFLAGS += -I$(STG_TOPDIR)/scaler/ip/blitter

# STM Blitter include path
EXTRA_CFLAGS += -I$(CONFIG_BLITTER_PATH)/linux/kernel/include/linux/stm/

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/scaler/ip/blitter/, \
	blitter_scaler.cpp                                         \
	blitter_task_node.cpp                                      \
	blitter_pool.cpp)
