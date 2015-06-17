# Get the source directory with symlinks resolved
STM_BLITTER_SOURCE_PATH = $(shell cd ..;/bin/pwd)
STM_BLITTER_TOPDIR=$(STLINUXTV_SOURCE_PATH)


export STM_BLITTER_SOURCE_PATH
export STM_BLITTER_TOPDIR

ARCH ?=sh

ifeq ($(ARCH),sh)
CROSS_COMPILE ?=sh4-linux-
else
ifeq ($(ARCH),armv7)
CROSS_COMPILE ?= armv7-linux-
endif
endif

# flexvp_ctrl, gtk  not built now
TEST_SUBDIRS = bltload

all: ACTION = all
all: $(TEST_SUBDIRS)

install: ACTION = install
install: $(TEST_SUBDIRS)

clean: ACTION = clean
clean: $(TEST_SUBDIRS)

.PHONY: $(TEST_SUBDIRS)
$(TEST_SUBDIRS):
	$(MAKE) -C $@ $(ACTION)
