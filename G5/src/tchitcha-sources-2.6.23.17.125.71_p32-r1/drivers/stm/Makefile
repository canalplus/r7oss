#
# Makefile for the STMicroelectronics device drivers.
#

obj-y				+= sysconf.o clk.o
obj-$(CONFIG_STM_DMA)		+= fdma.o fdma-xbar.o
obj-$(CONFIG_STM_PIO)		+= pio.o
obj-$(CONFIG_STM_PIO10)		+= pio10.o
obj-$(CONFIG_STM_COPROCESSOR_SUPPORT)	+= copro-st_socs.o
obj-$(CONFIG_STM_COPROCESSOR)		+= stm-coprocessor.o
obj-$(CONFIG_STM_COPROCESSOR_FRMW)	+= stm-coprocessor_frmw.o
obj-$(CONFIG_STM_RNG)		+= stm_rng.o
obj-y				+= emi.o
obj-$(CONFIG_PM)		+= platform-pm.o
obj-$(CONFIG_PMS)		+= pms.o
obj-$(CONFIG_STM_MIPHY)		+= miphy.o tap.o

ifdef CONFIG_STM_LPC
obj-$(CONFIG_CPU_SUBTYPE_STX5197) += lpc_5xxx.o
obj-$(CONFIG_CPU_SUBTYPE_STX5206) += lpc_7xxx.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111) += lpc_7xxx.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141) += lpc_7xxx.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200) += lpc_7xxx.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105) += lpc_7xxx.o
endif
