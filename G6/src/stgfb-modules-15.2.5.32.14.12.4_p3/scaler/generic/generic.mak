EXTRA_CFLAGS += -I$(STG_TOPDIR)/scaler/generic

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/scaler/generic/, \
	scaling_task_node.cpp                                   \
	scaling_pool.cpp                                        \
	scaling_queue.cpp                                       \
	scaler_device.cpp                                       \
	scaler.cpp)
