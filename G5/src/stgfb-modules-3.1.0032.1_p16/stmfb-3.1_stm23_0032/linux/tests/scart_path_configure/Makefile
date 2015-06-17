INC	   = -I$(STG_TOPDIR)  -I$(STG_TOPDIR)/../player2
CFLAGS = -g -Werror $(INC)
CROSS_COMPILE  := sh4-stlinux24-linux-gnu-
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
TARGET  = scart_path_configure
SRCS    = $(addprefix ./,$(TARGET).c)
OBJS    = $(patsubst %.c,%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)

dep:

clean :
	find . -name "*.o" -exec rm \{\} \;
	find . -name $(TARGET) -exec rm \{\} \;

$(OBJS): $(INCLUDES)

docs:
	doxygen Doxyfile
