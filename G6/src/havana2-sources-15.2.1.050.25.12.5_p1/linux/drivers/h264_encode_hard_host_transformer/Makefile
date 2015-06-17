ccflags-y += -I$(CONFIG_MULTICOM_PATH)/source/include

ccflags-y += -I$(TREE_ROOT)/linux/drivers/osdev_abs

#ccflags-y += -g
#ccflags-y += -O0

h264enchw-objs:= h264_encode_hard_host_transformer.o h264_encode_hard.o h264_encode_nvcl.o

obj-$(CONFIG_STM_STREAMINGENGINE) += h264enchw.o
