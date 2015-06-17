# Objects  we will link into busybox / subdirs we need to visit

CONFIG_KERNEL_BUILD?=$(KERNELDIR)
CONFIG_KERNEL_PATH?=$(KERNELDIR)

TREE_ROOT=$(CDIR)/

export CONFIG_KERNEL_BUILD CONFIG_KERNEL_PATH CONFIG_STGFB_PATH CONFIG_MODULES_PATH TREE_ROOT

export PLAYER2_PRIMARY_VIDEO_BUFFER_MEMORY
export PLAYER2_SECONDARY_VIDEO_BUFFER_MEMORY
export PLAYER2_AVR_VIDEO_BUFFER_MEMORY
export PLAYER2_H264_MACROBLOCK_STRUCTURE_MEMORY
export PLAYER2_H264_FRAME_MEMORY_SIZE
export PLAYER2_H264_STREAM_PARAMETERS_COUNT
export PLAYER2_H264_FRAME_PARAMETERS_COUNT
export PLAYER2_AUDIO_DEFAULT_CODED_MEMORY_SIZE
export PLAYER2_VIDEO_DEFAULT_CODED_MEMORY_SIZE

dir-y += player/

J?=-j 4

all:

ifeq ($(RPMIT),y)
SPEC?=SPECS/stm-noarchtarget-player2.spec
BUILDAREA?=$(PWD)/../

-include $(BUILDAREA)/Makefile.rpm
endif

distclean: clean
	-find . -name "*.o" | xargs rm -f
	-find . -name ".*.o.cmd" | xargs rm -f
	-find . -name ".*.ko.cmd" | xargs rm -f
	-find . -name "*.ko" | xargs rm -f
	-find . -name "*.mod.c" | xargs rm -f
	-rm -r doxygen
	-rm -f linux/.tmp_versions/*

modules:
	make -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/linux modules CROSS_COMPILE=$(CROSS_COMPILE) ARCH=sh $(J)

modules_install:
	make -C $(CONFIG_KERNEL_PATH) O=$(CONFIG_KERNEL_BUILD) M=`pwd`/linux modules_install CROSS_COMPILE=$(CROSS_COMPILE) ARCH=sh INSTALL_MOD_PATH=$(CONFIG_MODULES_PATH)

docs doxygen : .PHONY
	doxygen

.PHONY :


rpmpackage:
	-mkdir SOURCES
	tar cfj SOURCES/player2.tar.bz2 player README makefile.i386 makefile.sh4 makefile Makefile linux INSTALL Doxyfile COPYING components
