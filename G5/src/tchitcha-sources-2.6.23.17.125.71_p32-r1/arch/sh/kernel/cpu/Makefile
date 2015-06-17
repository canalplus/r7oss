#
# Makefile for the Linux/SuperH CPU-specifc backends.
#

obj-$(CONFIG_CPU_SH2)		= sh2/
obj-$(CONFIG_CPU_SH2A)		= sh2a/
obj-$(CONFIG_CPU_SH3)		= sh3/
obj-$(CONFIG_CPU_SH4)		= sh4/
obj-$(CONFIG_CPU_SH4A)		+= sh4a/

obj-$(CONFIG_UBC_WAKEUP)	+= ubc.o
obj-$(CONFIG_SH_ADC)		+= adc.o

ifdef CONFIG_STM_DRIVERS
obj-y	+= irq/ init.o
else
obj-y	+= irq/ init.o clock.o
endif

# Most chips still use the Hitachi standard CPG which has a few standard
# clocks. Those which do not can remove it here, and define a complete
# new clock architecture in their own processor specific code.
archclock-y					:= clock-cpg.o
archclock-$(CONFIG_CPU_SUBTYPE_FLI7510)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX5197)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX5206)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STB7100)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX7105)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX7108)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX7111)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX7141)	:=
archclock-$(CONFIG_CPU_SUBTYPE_STX7200)	:=
obj-y	+= $(archclock-y)
