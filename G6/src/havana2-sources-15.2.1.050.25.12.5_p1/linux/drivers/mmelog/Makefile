ccflags-y += -I$(CONFIG_MULTICOM_PATH)/source/include

ccflags-y += -I$(TREE_ROOT)/components/include
ccflags-y += -I$(TREE_ROOT)/linux/drivers/strelay

#due to current FCLOSE implementation
ccflags-y += -Wno-unused-but-set-variable
ccflags-y += -Wno-unused-but-set-parameter

mmelog-objs += acc_mme_wrapper.o

obj-$(CONFIG_STM_STREAMINGENGINE_MME) += mmelog.o
