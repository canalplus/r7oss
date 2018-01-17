AM_YFLAGS = -p $(PACKAGE)_yy

AM_LFLAGS  =
AM_LFLAGS += -P $(PACKAGE)_yy
if ENABLE_CONFIG_DEBUG
AM_LFLAGS += -d
endif

EXTRA_DIST  =
EXTRA_DIST += config_bison.y
EXTRA_DIST += config_flex.l

BUILT_SOURCES  =
BUILT_SOURCES += config_bison.c
BUILT_SOURCES += config_bison.h
BUILT_SOURCES += config_flex.c
BUILT_SOURCES += config_flex.h

CLEANFILES  =
CLEANFILES += config_bison.c
CLEANFILES += config_bison.h
CLEANFILES += config_flex.c
CLEANFILES += config_flex.h

config_bison.c config_bison.h: config_bison.y
	$(YACC) $(YFLAGS) $(AM_YFLAGS) -d -o $@ $<

config_flex.c config_flex.h: config_flex.l
	$(LEX) $(LFLAGS) $(AM_LFLAGS) --header-file=$(basename $@).h \
		-o $(basename $@).c $<

include_HEADERS = lxc_genesis.h

lib_LIBRARIES = liblxc_genesis_core.a

liblxc_genesis_core_a_SOURCES  =
liblxc_genesis_core_a_SOURCES += config_bison.c
liblxc_genesis_core_a_SOURCES += config_bison.h
liblxc_genesis_core_a_SOURCES += config_flex.c
liblxc_genesis_core_a_SOURCES += config_flex.h
liblxc_genesis_core_a_SOURCES += config.h
liblxc_genesis_core_a_SOURCES += core.c
liblxc_genesis_core_a_SOURCES += utils.c
liblxc_genesis_core_a_SOURCES += utils.h

liblxc_genesis_core_a_CFLAGS =
liblxc_genesis_core_a_CFLAGS += -fPIC
liblxc_genesis_core_a_CFLAGS += $(LIBCAP_CFLAGS)

lib_LIBRARIES += liblxc_genesis.a

liblxc_genesis_a_SOURCES = lxc_genesis.c

liblxc_genesis_a_CFLAGS  =
liblxc_genesis_a_CFLAGS += -fPIC
liblxc_genesis_a_CFLAGS += $(LIBCAP_CFLAGS)

liblxc_genesis_a_LIBADD = liblxc_genesis_core.a

sbin_PROGRAMS = lxc_genesis

lxc_genesis_SOURCES = main.c
