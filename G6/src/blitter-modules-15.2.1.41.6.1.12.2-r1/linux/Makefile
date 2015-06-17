# Get the source directory with symlinks resolved
export STM_BLITTER_TOPDIR := $(shell readlink -e ../)

KERNELDIR ?= $(error KERNELDIR is not set)

KBUILD_PATH := $(STM_BLITTER_TOPDIR)/linux/kernel
KBUILD_PATH_TESTS := $(STM_BLITTER_TOPDIR)/linux/tests

STM_TARGET_ARCH ?= sh4

ifeq ($(STM_TARGET_ARCH),sh4)
  CROSS_COMPILE ?= sh4-linux-
  ARCH ?= sh
else
  ifeq ($(STM_TARGET_ARCH),sh4_uclibc)
    CROSS_COMPILE ?= sh4-linux-uclibc-
    ARCH ?= sh
  else
    ifeq ($(STM_TARGET_ARCH),armv7)
      CROSS_COMPILE ?= armv7-linux-
      ARCH ?= arm
    endif
  endif
endif


UTILS_SUBDIRS = $(wildcard $(STM_BLITTER_TOPDIR)/linux/utils/*)

all: modules utils tests

modules:
	$(MAKE) -C $(KERNELDIR) $(patsubst %,O=%,$O) M="$(KBUILD_PATH)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" modules

.PHONY: modules_install
modules_install:
	$(MAKE) -C $(KERNELDIR) $(patsubst %,O=%,$O) M="$(KBUILD_PATH)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" INSTALL_MOD_DIR=kernel modules_install

.PHONY: tests
tests:
	$(MAKE) -C $(KERNELDIR) $(patsubst %,O=%,$O) M="$(KBUILD_PATH_TESTS)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" modules

.PHONY: tests_install
tests_install:
	$(MAKE) -C $(KERNELDIR) $(patsubst %,O=%,$O) M="$(KBUILD_PATH_TESTS)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" INSTALL_MOD_DIR=kernel modules_install

.PHONY: utils
.PHONY: $(UTILS_SUBDIRS)
utils: ACTION = all
utils: $(UTILS_SUBDIRS)

clean: ACTION = clean
clean: $(UTILS_SUBDIRS) $(TEST_SUBDIRS)
	find .. \( -name .git -prune \
			-o -name '.*.flags' -o -name '.depend' -o -name '.*.o.d' \
			-o -name '.*.cmd' -o -name '*.mod.c' -o -name '*.[oas]' \
			-o -name '*.ko' -o -name '*.map' -o -name '*.bak' \
			-o -name 'cfb*.c' -o -name Module.symvers \
			-o -name modules.order \) -type f -print | xargs rm -f
	find . -name .tmp_versions -type d -print | xargs rm -rf

$(UTILS_SUBDIRS) $(TEST_SUBDIRS):
	$(MAKE) -C $@ $(ACTION)
