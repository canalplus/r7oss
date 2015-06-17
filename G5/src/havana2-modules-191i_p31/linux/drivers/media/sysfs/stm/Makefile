CFLAGS += -I$(TREE_ROOT)/linux/include/linux/stm/ 
CFLAGS += -I$(TREE_ROOT)/linux/drivers/media/video/stm/
CFLAGS += -I$(TREE_ROOT)/linux/drivers/sound/pseudocard
CFLAGS += -I$(CONFIG_STGFB_PATH)/include 
CFLAGS += -DDEBUG_DVB

stmsysfs-objs:= player_interface.o \
		sysfs_module.o \
		player_sysfs.o

obj-$(CONFIG_STM_UNIFY_STMSYSFS) += stmsysfs.o
