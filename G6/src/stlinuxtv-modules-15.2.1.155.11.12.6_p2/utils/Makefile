# Get the source directory with symlinks resolved
STLINUXTV_SOURCE_PATH = $(shell cd ..;/bin/pwd)
STLINUXTV_TOPDIR=$(STLINUXTV_SOURCE_PATH)


export STLINUXTV_SOURCE_PATH
export STLINUXTV_TOPDIR

ARCH ?=sh

ifeq ($(ARCH),sh)
CROSS_COMPILE ?=sh4-linux-
else
ifeq ($(ARCH),armv7)
CROSS_COMPILE ?= armv7-linux-
endif
endif

# flexvp_ctrl, gtk  not built now
ifeq (y, $(filter y, $(CONFIG_STLINUXTV_DECODE_DISPLAY)))
TEST_SUBDIRS = v4l2lut8 v4l2gam v4l2layer_alignment v4l2stream v4l2vbi v4l2vbiplane zorder v4lEncodeTestApp tsmuxtest v4l2hdmi zero_grab v4l2cc v4l2grab_mmap v4l2hwjpegdecoder v4l2std v4l2alpha v4l2vxi hdmirx compo

endif

all: ACTION = all
all: $(TEST_SUBDIRS)

install: ACTION = install
install: $(TEST_SUBDIRS)

clean: ACTION = clean
clean: $(TEST_SUBDIRS)

.PHONY: $(TEST_SUBDIRS)
$(TEST_SUBDIRS):
	$(MAKE) -C $@ $(ACTION)
