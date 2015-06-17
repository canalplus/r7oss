#
# Makefile for memory devices
#

ifeq ($(CONFIG_DDR),y)
obj-$(CONFIG_OF)		+= of_memory.o
endif
obj-$(CONFIG_TI_EMIF)		+= emif.o
obj-$(CONFIG_TEGRA20_MC)	+= tegra20-mc.o
obj-$(CONFIG_TEGRA30_MC)	+= tegra30-mc.o
obj-$(CONFIG_ST_DDR_CNTRL)	+= st_ddr_pctl.o
