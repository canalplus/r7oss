ifneq ($(CONFIG_QUEUE_BUFFER),)

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/queuebufferinterface/,  \
						PureSwQueueBufferInterface.cpp)

endif
