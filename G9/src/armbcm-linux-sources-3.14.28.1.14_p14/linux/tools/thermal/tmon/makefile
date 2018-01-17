include $(ROOTDIR)/config.arch

LDFLAGS += -L$(ROOTDIR)/lib/ncurses/lib/
CFLAGS  += -I$(ROOTDIR)/lib/ncurses/include/

export CC CFLAGS LDFLAGS

all:
	$(MAKE) -f Makefile

all-% install-% clean-% distclean-%:
	$(MAKE) -f Makefile $@

clean:
	$(MAKE) -f Makefile $(COMMON_MAKEARGS) clean

distclean: clean
	-$(MAKE) -f Makefile $(COMMON_MAKEARGS) distclean

romfs:
	$(STRIP) tmon
	$(ROMFSINST) tmon /bin/tmon
