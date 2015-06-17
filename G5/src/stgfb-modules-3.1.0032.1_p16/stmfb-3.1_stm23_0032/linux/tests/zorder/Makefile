INC    = -I$(STG_TOPDIR)
CFLAGS = -D_GNU_SOURCE -Wall $(INC)
LDFLAGS = -lrt
CROSS_COMPILE  := sh4-stlinux24-linux-gnu-
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = zorder
SRCS    = $(addprefix ./,$(TARGET).c)
OBJS    = $(patsubst %.c,%.o,$(SRCS))

INCLUDES = $(STG_TOPDIR)/linux/drivers/video/stmfb.h \
           $(STG_TOPDIR)/linux/drivers/media/video/stmvout.h \
           $(STG_TOPDIR)/linux/tests/v4l2_helper.h

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)
