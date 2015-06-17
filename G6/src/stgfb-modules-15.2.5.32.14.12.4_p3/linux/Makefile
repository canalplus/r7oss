# Get the source directory with symlinks resolved
STMFB_ORIGINAL_SOURCE_PATH = $(shell cd ..;/bin/pwd)
STG_TOPDIR=$(STMFB_ORIGINAL_SOURCE_PATH)


export STMFB_ORIGINAL_SOURCE_PATH
export STG_TOPDIR
export CONFIG_STG_DEBUG CONFIG_STG_DEBUG_LEVEL
export INSTALL_MOD_PATH

KERNELDIR ?= $(error KERNELDIR is not set)

KBUILD_PATH = $(STG_TOPDIR)/linux/kernel

ARCH ?=sh

ifeq ($(ARCH),sh)
CROSS_COMPILE ?=sh4-linux-
else
ifeq ($(ARCH),arm)
CROSS_COMPILE ?= armv7-linux-
endif
endif

TEST_TMP1 = $(wildcard $(STG_TOPDIR)/linux/tests/*)
TEST_TMP2 = $(filter-out %/v4l2_helper.h,$(TEST_TMP1))
# temporary removed reglog test from compilation: this test should be used only for debug purposes
TEST_TMP4 = $(filter-out %/reglog,$(TEST_TMP2))
# temporary removed vcd test from compilation: this test should be used only for debug purposes
TEST_SUBDIRS = $(filter-out %/vcd,$(TEST_TMP4))

all: modules tests

modules:
	$(MAKE) -C $(KERNELDIR) $(patsubst %,O=%,$O) M="$(KBUILD_PATH)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" modules

.PHONY: modules_install
modules_install:
	$(MAKE) -C $(KERNELDIR) $(patsubst %,O=%,$O) M="$(KBUILD_PATH)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" INSTALL_MOD_DIR=kernel modules_install

tests: ACTION = all
tests: $(TEST_SUBDIRS)

tests_install: ACTION = install
tests_install: $(TEST_SUBDIRS)

clean: ACTION = clean
clean: $(TEST_SUBDIRS)
	find .. \( -name .git -prune \
			-o -name '.*.flags' -o -name '.depend' -o -name '.*.o.d' \
			-o -name '.*.cmd' -o -name '*.mod.c' -o -name '*.[oas]' \
			-o -name '*.ko' -o -name '*.map' -o -name '*.bak' \
			-o -name 'cfb*.c' -o -name Module.symvers \
			-o -name modules.order \) -type f -print | xargs rm -f
	find . -name .tmp_versions -type d -print | xargs rm -rf

.PHONY: dep
dep:
	$(MAKE) -C $(KERNELDIR) SUBDIRS="$(KERNEL_SUBDIRS)" dep

.PHONY: $(TEST_SUBDIRS)
$(TEST_SUBDIRS):
	$(MAKE) -C $@ $(ACTION)
