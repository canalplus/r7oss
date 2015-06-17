EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_INFRA_TOPDIR)/common/

obj-m	+= stm_power.o
POWER_OBJS := stm_power.o
obj-m	+= stm_power_hps_ir.o
HPS_OBJS := stm_power_hps_ir.o
ifeq ($(CONFIG_STM_LPM), y)
obj-m   += stm_power_ir.o
IR_SBC_OBJS := stm_power_ir.o
obj-m   += stm_power_pio.o
PIO_SBC_OBJS := stm_power_pio.o
endif

RELATIVE_ROOT = ../../
stm_power-objs := $(addprefix $(RELATIVE_ROOT)/stm_power/src/, $(POWER_OBJS))
stm_power_hps_ir-objs := $(addprefix $(RELATIVE_ROOT)/stm_power/src/, $(HPS_OBJS))
ifeq ($(CONFIG_STM_LPM), y)
stm_power_ir-objs := $(addprefix $(RELATIVE_ROOT)/stm_power/src/, $(IR_SBC_OBJS))
stm_power_pio-objs := $(addprefix $(RELATIVE_ROOT)/stm_power/src/, $(PIO_SBC_OBJS))
endif
