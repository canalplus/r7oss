STLINUXTV_TOPDIR=$(STLINUXTV_SOURCE_PATH)

STM_ARCH ?= arm
STM_TARGET_ARCH ?= armv7

STM_BASE_PREFIX ?= /opt/STM/STLinux-2.4
TARGET_DIR ?= $(STM_BASE_PREFIX)/devkit/$(STM_TARGET_ARCH)/target

INC		= -I $(STLINUXTV_TOPDIR)
CFLAGS += -Wall -Werror $(INC)
LDFLAGS += -lpthread
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = v4lEncodeTestApp
SRCS    = audio.c common.c v4lEncodeTestApp.c video.c crc.c
OBJS    = $(patsubst %.c,%.o,$(SRCS))

STLINUXTV_TOPDIR?=$(abspath ../../)

INCLUDES = $(STLINUXTV_TOPDIR)/linux/include/linux/stmvout.h \
           $(STLINUXTV_TOPDIR)/utils/v4l2_helper.h

prefix:=/usr

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

install: $(TARGET)
	install -m 755 -p $(TARGET) $(TARGET_DIR)/$(prefix)/bin/

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)
