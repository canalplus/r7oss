# Optionally, include config file to allow out of tree kernel modules build
#-include .config


DESTDIR=$(WORK)/STM/STLinux-2.4/devkit/armv7/target

# Core module
obj-$(CONFIG_SCART_ST6437)             += wyaavio.o
obj-$(CONFIG_SCART_ST6437)             += stv6430.o


#INSTALL SCART MODULES is done in ..modprobe.d/coredisplay.conf

