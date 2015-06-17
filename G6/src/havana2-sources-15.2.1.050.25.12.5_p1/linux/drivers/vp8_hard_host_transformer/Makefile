ccflags-y += -I$(CONFIG_MULTICOM_PATH)/source/include

ccflags-y += -I$(TREE_ROOT)/linux/drivers/osdev_abs

ccflags-y += -I$(TREE_ROOT)/linux/drivers/vp8_hard_host_transformer/inc

vp8hw-objs:= vp8_hard_host_transformer.o vp8hard.o dwl.o vp8hard_decode.o bits_operation.o

obj-$(CONFIG_STM_STREAMINGENGINE) += vp8hw.o
