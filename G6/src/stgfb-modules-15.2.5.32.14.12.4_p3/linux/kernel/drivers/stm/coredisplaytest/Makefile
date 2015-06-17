ifneq ($(CONFIG_STRELAY_PATH),)
obj-m += coredisplaytest.o

EXTRA_CFLAGS += -Werror

coredisplaytest-objs :=	stmcoredisplaytest.o \
					displaytestovp.o \
					circularbuffer.o \
					displaytestdbgfs.o \
					displaytestcmdparsing.o \
					displaytestresultshandling.o \
					displaytestivp.o\

CFLAGS_stmcoredisplaytest.o := -I$(CONFIG_STRELAY_PATH)
CFLAGS_displaytestovp.o := -I$(CONFIG_STRELAY_PATH)
CFLAGS_displaytestcmdparsing.o := -I$(CONFIG_STRELAY_PATH)
CFLAGS_displaytestresultshandling.o := -I$(CONFIG_STRELAY_PATH)
CFLAGS_displaytestivp.o := -I$(CONFIG_STRELAY_PATH)

endif