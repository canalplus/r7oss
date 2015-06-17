EXTRA_CFLAGS += -I$(TREE_ROOT)/linux/include/
EXTRA_CFLAGS += -I$(TREE_ROOT)/linux/drivers/media/dvb/frontends/
EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core/
EXTRA_CFLAGS += -Idrivers/media/dvb/frontends/
EXTRA_CFLAGS += -DCONFIG_DVB_HAVANA

stm-fei-objs:= st-fei.o 
stm-pti-objs:= st-pti-v2.o 
stm-merger-objs:= st-merger.o
stm-diseqc-objs:= st-diseqc.o

stm-common-objs:= st-common.o

obj-m += stm-pti.o  
obj-m += stm-fei.o
obj-m += stm-common.o
obj-m += stm-merger.o
obj-m += stm-diseqc.o
