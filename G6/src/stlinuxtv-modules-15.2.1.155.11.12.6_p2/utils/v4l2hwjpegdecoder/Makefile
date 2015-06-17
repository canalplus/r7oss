STLINUXTV_TOPDIR=$(STLINUXTV_SOURCE_PATH)

STM_ARCH ?= arm
STM_TARGET_ARCH ?= armv7

STM_BASE_PREFIX ?= /opt/STM/STLinux-2.4
TARGET_DIR ?= $(STM_BASE_PREFIX)/devkit/$(STM_TARGET_ARCH)/target

INC    = -I$(STLINUXTV_TOPDIR)
CFLAGS += -D_GNU_SOURCE -g -Wall $(INC)
LDFLAGS += -lrt
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc

PROGRAMS = v4l2-decode dfb-jpegdecode

V4L2_PROGRAMS_OBJS = $(patsubst v4l2%, v4l2%.o, $(PROGRAMS))
DFB_PROGRAMS_OBJS = $(patsubst dfb%, dfb%.o, $(PROGRAMS))

common.o: common.c
	$(CROSS_COMPILE)gcc -c $< $(CFLAGS) -I$(STLINUXTV_TOPDIR)  -lrt -o $@

v4l2%.o: v4l2%.c
	$(CROSS_COMPILE)gcc -c $< $(CFLAGS) -I$(STLINUXTV_TOPDIR)  -lrt -o $@

dfb%.o: dfb%.c
	$(CROSS_COMPILE)gcc -c $< $(CFLAGS) -I$(TARGET_DIR)/usr/include/directfb -I$(STLINUXTV_TOPDIR) -o $@

all: $(PROGRAMS)

v4l2%: v4l2%.o common.o
	$(CROSS_COMPILE)gcc $< common.o -o $@ $(LDFLAGS) -lrt

dfb%: dfb%.o common.o
	$(CROSS_COMPILE)gcc $< common.o -o $@ $(LDFLAGS) -ldirectfb -ldirect -lfusion -lrt

install:
	install -m 755 -p $(PROGRAMS) $(TARGET_DIR)/usr/bin/

clean:
	-find . -name "*.o" | xargs rm -f
	-find . -name ".*.o.cmd" | xargs rm -f
	-find . -name ".*.ko.cmd" | xargs rm -f
	-find . -name "*.ko" | xargs rm -f
	-find . -name "*.mod.c" | xargs rm -f
	-find . -name "*.symvers" | xargs rm -f
	-find . -name "*.order" | xargs rm -f
	rm -f $(PROGRAMS)
