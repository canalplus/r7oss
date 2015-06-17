INC	   = -I$(STG_TOPDIR)
CFLAGS = -Wall $(INC)
CROSS_COMPILE  := sh4-stlinux24-linux-gnu-
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = picture-cfg
SRCS    = $(addprefix ./,$(TARGET).c)
OBJS    = $(patsubst %.c,%.o,$(SRCS))

INCLUDES = $(STG_TOPDIR)/linux/drivers/video/stmfb.h

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)
