ifeq ($(CONFIG_MACH_STM_STIH407),y)

include $(STG_TOPDIR)/linux/kernel/drivers/stm/scaler/scaler.mak
include $(STG_TOPDIR)/scaler/soc/stiH407/soc.mak
include $(STG_TOPDIR)/scaler/ip/blitter/blitter.mak

# STM Blitter include path
EXTRA_CFLAGS += -I$(CONFIG_BLITTER_PATH)/linux/kernel/include/linux/stm/

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/linux/kernel/drivers/stm/scaler/stiH407/, \
  stiH407_scaler.c)

stiH407-scaler-objs := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
stiH407-scaler-objs := $(patsubst %.c,%.o,$(stiH407-scaler-objs))

obj-m += stmcore-scaler-stiH407.o

stmcore-scaler-stiH407-objs := $(stiH407-scaler-objs)

endif
