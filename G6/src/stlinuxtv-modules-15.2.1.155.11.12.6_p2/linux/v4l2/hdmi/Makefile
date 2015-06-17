ccflags-y += -I$(TREE_ROOT)/
ccflags-y += -I$(TREE_ROOT)/linux/v4l2/common
ccflags-y += -I$(CONFIG_STGFB_PATH)/include
ccflags-y += -I$(CONFIG_STGFB_PATH)/linux/kernel/include
ccflags-y += -I$(CONFIG_INFRA_PATH)/include
ccflags-y += -I$(CONFIG_INFRA_PATH)/stm_event/include
ccflags-y += -I$(CONFIG_INFRA_PATH)/common
ccflags-y += -Werror

obj-m += sthdmi.o
sthdmi-objs := stmhdmi.o stmhdmi_sysfs.o stmhdmi_edid.o stmhdmi_monitor.o
