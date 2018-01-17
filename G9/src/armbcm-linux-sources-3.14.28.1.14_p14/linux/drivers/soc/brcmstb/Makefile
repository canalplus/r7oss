obj-$(CONFIG_BRCMSTB_PM)	+= pm/

obj-y				+= common.o biuctrl.o
obj-$(CONFIG_BRCMSTB_BMEM)	+= bmem.o
obj-$(CONFIG_BRCMSTB_CMA)	+= cma_driver.o
obj-$(CONFIG_BRCMSTB_IRQ0_STUB)	+= irq0-stub.o
obj-$(CONFIG_BRCMSTB_MEMORY_API) += memory.o
obj-$(CONFIG_BRCMSTB_SRPD)	+= srpd.o
obj-$(CONFIG_BRCMSTB_WKTMR)	+= wktmr.o
obj-$(CONFIG_BRCMSTB_NEXUS_API)	+= nexus/

CFLAGS_memory.o += -I$(src)/../../../scripts/dtc/libfdt
