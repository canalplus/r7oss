#
# Makefile for ALSA driver
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@perex.cz>
#

ifneq ($(KERNELRELEASE),)
# call from 2.6 kernel build system

obj-m += acore/ i2c/ drivers/ isa/ pci/ ppc/ arm/ synth/ usb/ sparc/ parisc/ sh/ pcmcia/ aoa/ soc/ misc/

else

ALSAKERNELDIR = ../alsa-kernel

ifndef IGROUP
IGROUP = root
endif
ifndef IUSER
IUSER = root
endif

ifeq (Makefile.conf,$(wildcard Makefile.conf))
include Makefile.conf
include toplevel.config
else
.PHONY: dummy1
dummy1:
	$(MAKE) all-deps
	@echo
	@echo "Please, run the configure script as first..."
	@echo
MAINSRCDIR := $(shell /bin/pwd)
endif

SND_TOPDIR   = $(MAINSRCDIR)
export SND_TOPDIR

ifeq (,$(wildcard acinclude.m4))
.PHONY: dummy2
dummy2:
	$(MAKE) all-deps
	@echo
	@echo "Please, run the configure script as first..."
	@echo
endif

ifdef NEW_KBUILD
ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
  endif
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif
ifdef C
  ifeq ("$(origin C)", "command line")
    KBUILD_CHECKSRC = $(C)
  endif
endif
ifndef KBUILD_CHECKSRC
  KBUILD_CHECKSRC = 0
endif
export KBUILD_VERBOSE KBUILD_CHECKSRC
endif

SUBDIRS  = acore i2c drivers isa synth pci aoa soc
CSUBDIRS =

ifndef NEW_KBUILD
SUBDIRS += support
endif

ifeq (y,$(CONFIG_ARM))
SUBDIRS  += arm
endif
ifeq (y,$(CONFIG_PPC))
SUBDIRS  += ppc
endif
ifeq (y,$(CONFIG_SPARC32))
SUBDIRS  += sparc
else
ifeq (y,$(CONFIG_SPARC64))
SUBDIRS  += sparc
endif
endif
ifeq (y,$(CONFIG_MIPS))
SUBDIRS  += mips
endif
ifeq (y,$(CONFIG_SGI))
SUBDIRS  += hal2
endif
ifeq (y,$(CONFIG_USB))
SUBDIRS  += usb
endif
ifeq (y,$(CONFIG_PCMCIA))
SUBDIRS  += pcmcia
endif
ifeq (y,$(CONFIG_PARISC))
SUBDIRS  += parisc
endif
ifeq (y,$(CONFIG_SUPERH))
SUBDIRS  += sh
endif
ifeq (m,$(CONFIG_AC97_BUS))
SUBDIRS  += misc
endif
CSUBDIRS += include test utils

KCONFIG_FILES = $(shell find $(SND_TOPDIR) -name Kconfig) $(shell find $(SND_TOPDIR)/alsa-kernel/ -name Kconfig)

.PHONY: all
all: compile

alsa-kernel/sound_core.c:
	ln -sf $(ALSAKERNELDIR) alsa-kernel
	ln -sf alsa-kernel sound
	ln -sf alsa-kernel/scripts scripts

include/sound/version.h: include/version.h
	if [ ! -d include/sound -a ! -L include/sound ]; then \
	  ln -sf ../alsa-kernel/include include/sound ; \
	fi
	cp -puvf include/version.h include/sound/version.h

utils/mod-deps: utils/mod-deps.c
	gcc utils/mod-deps.c -o utils/mod-deps

toplevel.config.in: $(KCONFIG_FILES) alsa-kernel/sound_core.c utils/mod-deps
	utils/mod-deps --basedir $(SND_TOPDIR)/alsa-kernel --hiddendir $(SND_TOPDIR) --versiondep $(SND_TOPDIR)/kconfig-vers --makeconf > toplevel.config.in

acinclude.m4: $(KCONFIG_FILES) alsa-kernel/sound_core.c utils/mod-deps
	utils/mod-deps --basedir $(SND_TOPDIR)/alsa-kernel --hiddendir $(SND_TOPDIR) --versiondep $(SND_TOPDIR)/kconfig-vers --acinclude > acinclude.m4

include/config1.h.in: $(KCONFIG_FILES) alsa-kernel/sound_core.c utils/mod-deps
	utils/mod-deps --basedir $(SND_TOPDIR)/alsa-kernel --hiddendir $(SND_TOPDIR) --versiondep $(SND_TOPDIR)/kconfig-vers --include > include/config1.h.in

all-deps: toplevel.config.in acinclude.m4 include/config1.h.in

include/sndversions.h:
	$(MAKE) dep

.PHONY: compile
compile: include/sound/version.h include/sndversions.h
ifdef NEW_KBUILD
	$(MAKE) -C $(CONFIG_SND_KERNELDIR) SUBDIRS=$(MAINSRCDIR) $(MAKE_ADDS) CPP="$(CPP)" CC="$(CC)" modules
	utils/link-modules $(SND_TOPDIR)
else
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d; then exit 1; fi; done
endif
	@echo
	@echo "ALSA modules were successfully compiled."
	@echo

.PHONY: dep
dep: include/sound/version.h
ifdef NEW_KBUILD
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d prepare; then exit 1; fi; done
else
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d fastdep; then exit 1; fi; done
endif

.PHONY: map
map:
	awk "{ if ( length( $$1 ) != 0 ) print $$1 }" snd.map | sort -o snd.map1
	mv -f snd.map1 snd.map

.PHONY: install
install: install-headers install-modules install-scripts check-snd-prefix
	@if [ -L /dev/snd ]; then \
		echo "The ALSA devices were removed from /proc/asound/dev directory." ; \
		echo "Creating static device entries in /dev/snd." ; \
		$(SND_TOPDIR)/snddevices ; \
	fi
	cat WARNING

.PHONY: install-headers
install-headers:
	if [ -L $(DESTDIR)$(prefix)/include/sound ]; then \
		rm -f $(DESTDIR)$(prefix)/include/sound; \
		ln -sf $(MAINSRCDIR)/include/sound $(DESTDIR)$(prefix)/include/sound; \
	else \
		rm -rf $(DESTDIR)$(prefix)/include/sound; \
		install -d -m 755 -g $(IGROUP) -o $(IUSER) $(DESTDIR)$(prefix)/include/sound; \
		for f in include/sound/*.h; do \
			install -m 644 -g $(IGROUP) -o $(IUSER) $$f $(DESTDIR)$(prefix)/include/sound; \
		done \
	fi

ifeq ($(CONFIG_SND_KERNELDIR)/System.map,$(wildcard $(CONFIG_SND_KERNELDIR)/System.map))
SYSTEM_MAP_OPT = -F $(CONFIG_SND_KERNELDIR)/System.map
endif

.PHONY: install-modules
install-modules:
ifeq ($(moddir_tree),y)
	find $(DESTDIR)$(moddir) -name 'snd*.*o' | xargs rm -f
	find $(DESTDIR)$(moddir) -name 'snd*.*o.gz' | xargs rm -f
	find $(DESTDIR)$(moddir) -name 'ac97_bus.*o' | xargs rm -f
	find $(DESTDIR)$(moddir) -name 'ac97_bus.*o.gz' | xargs rm -f
else
	rm -f $(DESTDIR)$(moddir)/snd*.*o $(DESTDIR)$(moddir)/persist.o $(DESTDIR)$(moddir)/isapnp.o
	rm -f $(DESTDIR)$(moddir)/snd*.*o.gz $(DESTDIR)$(moddir)/persist.o.gz $(DESTDIR)$(moddir)/isapnp.o.gz
endif
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d modules_install; then exit 1; fi; done
ifeq ($(DESTDIR),)
	-/sbin/depmod -a $(kaversion) $(SYSTEM_MAP_OPT)
else
	@echo "*** This is a \`staged' install using the prefix"
	@echo "***	\"$(DESTDIR)\"."
	@echo "***"
	@echo "*** Once the modules have been moved to their final destination you must run the command"
	@echo "***	\"/sbin/depmod -a $(kaversion) $(SYSTEM_MAP_OPT)\"."
	@echo "***"
	@echo "*** Alternatively, if you build a package (e.g. rpm), include the"
	@echo "*** depmode command above in the post-(un)install procedure."
endif

.PHONY: install-scripts
install-scripts:
	@for d in /sbin/init.d /etc/rc.d/init.d /etc/init.d; do \
	 if [ -d $(DESTDIR)$$d ]; then \
	   if [ -f $(DESTDIR)$$d/alsasound ]; then \
	     cmp -s utils/alsasound $(DESTDIR)$$d/alsasound || \
	       install -m 755 -g $(IGROUP) -o $(IUSER) utils/alsasound $(DESTDIR)$$d/alsasound.new; \
	     break; \
	   fi; \
	   install -m 755 -g $(IGROUP) -o $(IUSER) utils/alsasound $(DESTDIR)$$d/alsasound; \
	   break; \
	fi; done

.PHONY: check-snd-prefix
check-snd-prefix:
	@ if [ x"$(DESTDIR)" = x -a -f /etc/modules.conf ]; then \
	  if grep -s -q snd_ /etc/modules.conf; then \
	    echo;\
	    echo "             ===== WARNING =====";\
	    echo;\
	    echo "The options for ALSA modules on your system still include snd_ prefix,";\
	    echo "which is obsoleted now.  Please fix /etc/modules.conf.";\
	    echo "For convenience, you can use utils/module-options script to convert";\
	    echo "the snd_ prefix automatically.";\
	    echo;\
	fi; fi

.PHONY: clean1
clean1:
	rm -f .depend *.o snd.map*
	rm -f $(DEXPORT)/*.ver
	rm -f modules/*.o modules/*.ko

.PHONY: clean
clean: clean1
ifdef NEW_KBUILD
	rm -rf .tmp_versions
endif
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d clean; then exit 1; fi; done
	@for d in $(CSUBDIRS); do if ! $(MAKE) -C $$d clean; then exit 1; fi; done

.PHONY: mrproper
mrproper: clean1
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d mrproper; then exit 1; fi; done
	@for d in $(CSUBDIRS); do if ! $(MAKE) -C $$d mrproper; then exit 1; fi; done
	rm -f *~ out.txt *.orig *.rej .#* .gdb_history
	rm -f doc/*~
	rm -f config.cache config.log config.status Makefile.conf
	rm -f utils/alsa-driver.spec
	rm -f `find ../alsa-kernel -name "*~"`
	rm -f `find ../alsa-kernel -name "*.orig"`
	rm -f `find ../alsa-kernel -name "*.rej"`
	rm -f `find ../alsa-kernel -name ".#*"`
	rm -f `find ../alsa-kernel -name "out.txt"`
	rm -rf autom4te.cache

.PHONY: cvsclean hgclean
cvsclean: mrproper
	rm -f configure snddevices aclocal.m4 acinclude.m4 include/config.h include/config1.h \
	  include/config1.h.in toplevel.config toplevel.config.in \
	  alsa-kernel sound scripts include/sound
	rm -rf include/linux
hgclean: cvsclean

.PHONY: pack
pack: mrproper
	chmod 755 utils/alsasound
	# big note: use always '--bzip2 -p' where -p is fake argument
	# it seems that some older tar has wrong getopt list
	{ \
		cd .. ; \
		rm -f alsa-driver/alsa-kernel ; \
		mv alsa-kernel alsa-driver ; \
		mv alsa-driver alsa-driver-$(CONFIG_SND_VERSION) ; \
		tar --exclude=CVS --exclude=kchanges --exclude=.cvsignore \
                    --exclude='.hg*' \
                    --owner=$(IGROUP) --group=$(IUSER) -cv --bzip2 -p \
                    -f alsa-driver-$(CONFIG_SND_VERSION).tar.bz2 alsa-driver-$(CONFIG_SND_VERSION) ; \
		mv alsa-driver-$(CONFIG_SND_VERSION) alsa-driver ; \
		mv alsa-driver/alsa-kernel . ; \
	}

.PHONY: uninstall
uninstall:
	-rm -rf $(DESTDIR)$(prefix)/include/sound
ifeq ($(moddir_tree),y)
	{ \
		for i in core $(SUBDIRS) ; do \
			rm -rf $(DESTDIR)$(moddir)/$$i ; \
		done ; \
	}
else
	rm -f $(DESTDIR)$(moddir)/snd*.o $(DESTDIR)$(moddir)/persist.o $(DESTDIR)$(moddir)/isapnp.o
endif
	rm -f $(DESTDIR)/sbin/init.d/alsasound
	rm -f $(DESTDIR)/etc/rc.d/init.d/alsasound
	rm -f $(DESTDIR)/etc/init.d/alsasound

.PHONY: TAGS
TAGS:
	find . -name '*.[ch]' | etags -

endif
