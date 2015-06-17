STLINUXTV_TOPDIR=$(STLINUXTV_SOURCE_PATH)

STM_ARCH ?= arm
STM_TARGET_ARCH ?= armv7

STM_BASE_PREFIX ?= /opt/STM/STLinux-2.4
TARGET_DIR ?= $(STM_BASE_PREFIX)/devkit/$(STM_TARGET_ARCH)/target

PKG_CONFIG ?= $(STM_BASE_PREFIX)/host/bin/pkg-config
PKG_CONFIG_LIBDIR ?= $(STM_BASE_PREFIX)/devkit/armv7/target/usr/lib/pkgconfig
PKG_CONFIG_SYSROOT_DIR ?= $(STM_BASE_PREFIX)/devkit/armv7/target

INC		= -I $(STLINUXTV_TOPDIR)/linux/include \
		  -I $(STLINUXTV_TOPDIR) \
                  -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/ \
		  `PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
			PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) \
			$(PKG_CONFIG) --cflags \
			directfb`
CFLAGS		+= -Wall -Werror $(INC)
LDFLAGS		+= `PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
			PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) \
			$(PKG_CONFIG) --libs \
			directfb`

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = zerograb
SRCS    = zerograb.c
OBJS    = $(patsubst %.c,%.o,$(SRCS))

INCLUDES = $(STLINUXTV_TOPDIR)/utils/v4l2_helper.h

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

install: $(TARGET)
	install -m 755 -p $(TARGET) $(TARGET_DIR)/usr/bin/

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)
