ifeq ($(CONFIG_MACH_STM_STIH416),y)

include $(STG_TOPDIR)/linux/kernel/drivers/stm/scaler/scaler.mak
include $(STG_TOPDIR)/scaler/soc/stiH416/soc.mak
include $(STG_TOPDIR)/scaler/ip/blitter/blitter.mak
include $(STG_TOPDIR)/scaler/ip/fvdp/fvdp.mak

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/scaler/stiH416/, \
  stiH416_scaler.c)

stiH416-scaler-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
stiH416-scaler-objs := $(patsubst %.c,%.o,$(stiH416-scaler-objs))

obj-m += stmcore-scaler-stiH416.o

stmcore-scaler-stiH416-objs := $(stiH416-scaler-objs)

endif

