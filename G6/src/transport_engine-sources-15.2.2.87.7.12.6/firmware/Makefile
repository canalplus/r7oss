# Transport engine firmware Makefile
#
# Variables:
# INSTALL_PATH - Path to install firmwares into

# Set some sensible defaults for variables
STM_BASE_PREFIX ?= /opt/STM/STLinux-2.4
ARCH ?= armv7
STM_CROSS_TARGET_DIR ?= $(STM_BASE_PREFIX)/devkit/$(ARCH)/target
INSTALL_PATH ?= $(STM_CROSS_TARGET_DIR)/lib/firmware
export STM_TE_TOPDIR := $(shell readlink -e ../)

#
# Global targets
#
.PHONY: all clean install
all: tp

install: tp_install memdma_install

clean: tp_clean

#
# TP firmware targets
#
.PHONY: tp tp_install tp_clean
tp:
	$(MAKE) -f Makefile.fw TP

tp_install:
	cp pti_tp*.elf $(INSTALL_PATH)/

tp_clean:
	$(MAKE) -f Makefile.fw clean_TP

#
# MEMDMA firmware targets
#
.PHONY: memdma_install

memdma_install:
	cp pti_memdma_*.elf $(INSTALL_PATH)/
