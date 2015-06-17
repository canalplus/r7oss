STLINUXTV_TOPDIR=$(STLINUXTV_SOURCE_PATH)

STM_ARCH ?= arm
STM_TARGET_ARCH ?= armv7

STM_BASE_PREFIX ?= /opt/STM/STLinux-2.4
TARGET_DIR ?= $(STM_BASE_PREFIX)/devkit/$(STM_TARGET_ARCH)/target

INC    = -I$(STLINUXTV_TOPDIR) -I$(STLINUXTV_TOPDIR)/utils
CFLAGS += -D_GNU_SOURCE -g -Wall -Werror $(INC)
LDFLAGS += -lrt -lmediactl -lv4l2subdev -lpthread
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = hdmirx-capture
SRCS    = $(wildcard *.c)
OBJS    = $(patsubst %.c,%.o,$(SRCS))

ifneq ($(CONFIG_STG_OPTLEVEL),)
  CFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif

INCLUDES = $(STLINUXTV_TOPDIR)/linux/include/linux/stmvout.h \
           $(STLINUXTV_TOPDIR)/utils/v4l2_helper.h

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

install: $(TARGET)
	install -m 755 -p $(TARGET) $(TARGET_DIR)/usr/bin/
	install -m 755 -p plane.conf $(TARGET_DIR)/root/

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)
