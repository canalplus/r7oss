obj-$(CONFIG_SMSC911x) := smsc9118.o

pf-$(CONFIG_CPU_SUBTYPE_ST40)	:= st40.c
pf-$(CONFIG_CPU_XSCALE)	:= xscale_linux_2.4.c

CFLAGS_smsc9118.o += -DUSE_DEBUG
CFLAGS_smsc9118.o += -DPLATFORM_SOURCE=\"$(pf-y)\"
