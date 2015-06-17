INC    = -I$(STLINUXTV_TOPDIR)
CFLAGS += -D_GNU_SOURCE -g -Wall $(INC)
LDFLAGS += -lrt -lmediactl -lv4l2subdev
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = v4l2Windowtest
SRCS    = $(addprefix ./,$(TARGET).c)
OBJS    = $(patsubst %.c,%.o,$(SRCS))

ifneq ($(CONFIG_STG_OPTLEVEL),)
  CFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif

INCLUDES = $(STLINUXTV_TOPDIR)/utils/v4l2_helper.h

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)
