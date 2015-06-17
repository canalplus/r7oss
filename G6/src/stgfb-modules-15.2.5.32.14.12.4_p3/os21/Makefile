STG_TOPDIR=$(shell pwd)/..

LIB_SUBDIR = $(STG_TOPDIR)/os21/stglib

TEST_TMP = $(wildcard $(STG_TOPDIR)/os21/tests/*)
TEST_SUBDIRS = $(filter-out %/CVS,$(TEST_TMP))


export STG_TOPDIR LIB_SUBDIR

CFLAGS = -I$(STG_TOPDIR) -I$(STG_TOPDIR)/include -I$(STG_TOPDIR)/private/include -mruntime=os21 -Wall
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


ifeq ($(CONFIG_STIH407_DUMMY),y)
CONFIG_STIH407=y
CFLAGS  += -DCONFIG_STI7108=y -DCONFIG_HDK7108=y
LDFLAGS += -mboard=hdk7108_hostse
else
$(warning  Define CONFIG_STB7111_MB618,)
$(warning         CONFIG_STB7141_MB628,)
$(warning         CONFIG_STB7105_MB680,)
$(warning         CONFIG_STB7106_HDK7106,)
$(warning         CONFIG_STB7108_HDK7108,)
$(error           CONFIG_STB7108_MB837 to = y)
endif

export CC AR LD CFLAGS ARFLAGS LDFLAGS
export CONFIG_STIH407

all: lib tests

lib:
	$(MAKE) -C $(LIB_SUBDIR)

tests: ACTION = all
tests: $(TEST_SUBDIRS)

clean: ACTION = clean
clean: $(TEST_SUBDIRS)
	find .. \( -name '.*.flags' -o -name '.depend' -o -name '.*.o.d' -o -name '.*.cmd' -o -name '*.mod.c' \
			-o -name '*.[oas]' -o -name '*.ko' -o -name '*.map' -o -name '*.bak' \) -type f -print | xargs rm -f
	find . -name .tmp_versions -type d -print | xargs rm -rf

clean_lib:
	$(MAKE) clean -C $(STG_TOPDIR)

.PHONY: $(TEST_SUBDIRS)
$(TEST_SUBDIRS):
	$(MAKE) -C $@ $(ACTION)
