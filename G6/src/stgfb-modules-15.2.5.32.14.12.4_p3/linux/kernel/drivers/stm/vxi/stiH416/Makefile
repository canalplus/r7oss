ifeq ($(CONFIG_CPU_SUBTYPE_STIH416),y)

ifeq ($(CONFIG_ARM),y)
KBUILD_CPPFLAGS += -DCONFIG_STM_HOST_CPU
endif

include $(STG_TOPDIR)/linux/kernel/drivers/stm/vxi/vxi.mak

include $(STG_TOPDIR)/vxi/soc/stiH416/soc.mak

stiH416-vxi-objs := $(addsuffix .o,$(basename $(STM_VXI_SRC_FILES)))

obj-m += stm-vxi-stiH416.o

stm-vxi-stiH416-objs := $(stiH416-vxi-objs)
stm-vxi-stiH416-objs += stiH416.o

endif
