STG_TOPDIR=$(shell pwd)/..

LIB_SUBDIR = $(STG_TOPDIR)/os21/stglib
			
TEST_TMP = $(wildcard $(STG_TOPDIR)/os21/tests/*)
TEST_SUBDIRS = $(filter-out %/CVS,$(TEST_TMP))


export STG_TOPDIR LIB_SUBDIR

CFLAGS = -I$(STG_TOPDIR) -I$(STG_TOPDIR)/include -mruntime=os21 -Wall
LDFLAGS=-mruntime=os21 -L$(LIB_SUBDIR) -lstgos21

ifeq ($(ARCH),sh)

CC=sh4gcc
LD=sh4g++
AR=sh4ar

CFLAGS += -Wno-format

ifeq ($(CONFIG_STG_DEBUG),y)
CFLAGS += -g -DDEBUG
else
CFLAGS += -Os -g
endif
ARFLAGS=rcs

else
$(error Unspecified build architecture, set ARCH=sh)
endif


# STB7100 MB411
ifeq ($(CONFIG_STB7100_MB411),y)
CONFIG_STB7100=y
CFLAGS  += -DCONFIG_STB7100=y
LDFLAGS += -mboard=mb411p1
else
# STB7100 MB442
ifeq ($(CONFIG_STB7100_MB442),y)
CONFIG_STB7100=y
CFLAGS  += -DCONFIG_STB7100=y
LDFLAGS += -mboard=mb442p1
else
# STB5202 MB602
ifeq ($(CONFIG_STI5202_MB602),y)
CONFIG_STB7100=y
CFLAGS  += -DCONFIG_STI5202=y -DCONFIG_STB7100=y -DCONFIG_MB602=y
LDFLAGS += -mboard=mb602p1
else
# STi7200 MB671
ifeq ($(CONFIG_STI7200_MB671),y)
CONFIG_STI7200C2=y
CFLAGS  += -DCONFIG_STI7200=y -DCONFIG_MB671=y
LDFLAGS += -mboard=mb671p1
else
# STi7111 MB618
ifeq ($(CONFIG_STI7111_MB618),y)
CONFIG_STI7111=y
CFLAGS  += -DCONFIG_STI7111=y -DCONFIG_MB618=y
LDFLAGS += -mboard=mb618p1
else
ifeq ($(CONFIG_STI7141_MB628),y)
CONFIG_STI7141=y
CFLAGS  += -DCONFIG_STI7141=y
LDFLAGS += -mboard=mb628_estbp1
else
ifeq ($(CONFIG_STI7105_MB680),y)
CONFIG_STI7105=y
CFLAGS  += -DCONFIG_STI7105=y -DCONFIG_MB680=y
LDFLAGS += -mboard=mb680p1
else
ifeq ($(CONFIG_STI7105_SDK7105),y)
CONFIG_STI7105=y
CFLAGS  += -DCONFIG_STI7105=y -DCONFIG_SDK7105=y
LDFLAGS += -mboard=mb680p1
else
ifeq ($(CONFIG_STI7106_MB680),y)
# Pin compatible with 7105 and MB680 for our purposes.
CONFIG_STI7106=y
CFLAGS  += -DCONFIG_STI7106=y -DCONFIG_MB680=y
LDFLAGS += -mboard=mb680p1
else
ifeq ($(CONFIG_STI5289_MB796),y)
# Identical to 5206 for this purspose.
CONFIG_STI5206=y
CFLAGS  += -DCONFIG_STI5206=y -DCONFIG_MB796=y
LDFLAGS += -mboard=mb796sti5289p1
else
ifeq ($(CONFIG_STI7108_MB837),y)
CONFIG_STI7108=y
CFLAGS  += -DCONFIG_STI7108=y -DCONFIG_MB837=y
LDFLAGS += -mboard=mb837_hostse
else
$(warning  Define CONFIG_STB7100_MB411)
$(warning         CONFIG_STB7100_MB442,)
$(warning         CONFIG_STI5202_MB602,)
$(warning         CONFIG_STI5289_MB796,)
$(warning         CONFIG_STB7111_MB618,)
$(warning         CONFIG_STB7141_MB628,)
$(warning         CONFIG_STB7105_MB680,)
$(warning         CONFIG_STB7106_MB680,)
$(warning         CONFIG_STB7108_MB837,)
$(error           or CONFIG_STI7200_MB671 to = y)
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif

export CC AR LD CFLAGS ARFLAGS LDFLAGS CONFIG_STB7100 CONFIG_STI7200C1 
export CONFIG_STI7200C2 CONFIG_STI7111 CONFIG_STI7141 CONFIG_STI7105
export CONFIG_STI7106 CONFIG_STI7108 CONFIG_STI5206

all: lib tests

lib:
	make -C $(LIB_SUBDIR)
        
tests: ACTION = all
tests: $(TEST_SUBDIRS)

clean: ACTION = clean
clean: $(TEST_SUBDIRS)
	find .. \( -name '.*.flags' -o -name '.depend' -o -name '.*.o.d' -o -name '.*.cmd' -o -name '*.mod.c' \
			-o -name '*.[oas]' -o -name '*.ko' -o -name '*.map' -o -name '*.bak' \) -type f -print | xargs rm -f
	find . -name .tmp_versions -type d -print | xargs rm -rf


.PHONY: $(TEST_SUBDIRS)
$(TEST_SUBDIRS):
	make -C $@ $(ACTION)
