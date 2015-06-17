SRCROOT = .

# kbuild compatibility
export srctree  := $(shell pwd)
export objtree  := $(shell pwd)
export KLIBCSRC := usr/klibc
export VERSION := $(shell cat $(KLIBCSRC)/version)
export KLIBCINC := usr/include
export KLIBCOBJ := usr/klibc
export KLIBCKERNELSRC := linux/
export KLIBCKERNELOBJ := linux/
include scripts/Kbuild.include

KLIBCROSS	?= $(CROSS_COMPILE)
export KLIBCROSS
export CC	:= $(KLIBCROSS)gcc
export LD	:= $(KLIBCROSS)ld
export AR	:= $(KLIBCROSS)ar
export RANLIB	:= $(KLIBCROSS)ranlib
export STRIP	:= $(KLIBCROSS)strip
export NM	:= $(KLIBCROSS)nm
export OBJCOPY  := $(KLIBCROSS)objcopy
export OBJDUMP  := $(KLIBCROSS)objdump

NOSTDINC_FLAGS := -nostdlib -nostdinc -isystem $(shell $(CC) -print-file-name=include)

ARCH	          := $(shell uname -m | sed -e s/i.86/i386/ -e s/parisc64/parisc/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/sh4/sh/)
export KLIBCARCH  ?= $(ARCH)
export KLIBCARCHDIR := $(shell echo $(KLIBCARCH) | sed -e s/s390x/s390/)

export HOSTCC     := gcc
export HOSTCFLAGS := -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer
export PERL       := perl

# Location for installation
export prefix      = /usr
export bindir      = $(prefix)/bin
export libdir      = $(prefix)/lib
export mandir      = $(prefix)/man
export INSTALLDIR  = $(prefix)/lib/klibc
export INSTALLROOT =

# Create a fake .config as present in the kernel tree
# But if it exists leave it alone
$(if $(wildcard $(objtree)/.config),,$(shell cp defconfig .config))

# Prefix Make commands with $(Q) to silence them
# Use quiet_cmd_xxx, cmd_xxx to create nice output
# use make V=1 to get verbose output

ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
  endif
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

# If the user is running make -s (silent mode), suppress echoing of
# commands

ifneq ($(findstring s,$(MAKEFLAGS)),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

# Shorthand to call Kbuild.klibc
klibc := -f $(srctree)/scripts/Kbuild.klibc obj

# Very first target
.PHONY: all klcc klibc
all: klcc klibc

.config: defconfig linux
	@echo "defconfig has changed, please remove or edit .config"
	@false

linux:
	@echo "The 'linux' symlink is missing; it should point to a kernel tree "
	@echo "configured for the $(KLIBCARCH) architecture."
	@false

rpmbuild = $(shell which rpmbuild 2>/dev/null || which rpm)

klibc.spec: klibc.spec.in $(KLIBCSRC)/version
	sed -e 's/@@VERSION@@/$(VERSION)/g' < $< > $@

# Build klcc - it is the first target
klcc: .config
	$(Q)$(MAKE) $(klibc)=klcc

klibc: .config
	$(Q)$(MAKE) $(klibc)=.

test: klibc
	$(Q)$(MAKE) $(klibc)=usr/klibc/tests


###
# allow one to say make dir/file.o
# Caveat: works only for .c files where we have a Kbuild file in same dir
%.o: %.c FORCE
	 $(Q)$(MAKE) $(klibc)=$(dir $<) $(dir $<)$(notdir $@)

%.s: %.c FORCE
	 $(Q)$(MAKE) $(klibc)=$(dir $<) $(dir $<)$(notdir $@)

%.i: %.c FORCE
	 $(Q)$(MAKE) $(klibc)=$(dir $<) $(dir $<)$(notdir $@)

FORCE: ;
###
# clean: remove generated files
# mrproper does a full cleaning including .config and linux symlink
FIND_IGNORE := \( -name .git \) -prune -o
quiet_cmd_rmfiles = $(if $(wildcard $(rm-files)),RM     $(wildcard $(rm-files)))
      cmd_rmfiles = rm -f $(rm-files)
clean:
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.clean obj=.
	$(Q)find . $(FIND_IGNORE) \
		\( -name *.o -o -name *.a -o -name '.*.cmd' -o \
		   -name '.*.d' -o -name '.*.tmp' \) \
		-type f -print | xargs rm -f

rm-files := .config linux
distclean mrproper: clean
	 $(Q)find . $(FIND_IGNORE) \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -size 0 \
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \) \
		-type f -print | xargs rm -f
	$(call cmd,rmfiles)

install: all
	$(Q)$(MAKE) -f $(srctree)/scripts/Kbuild.install obj=.

# This does all the prep work needed to turn a freshly exported git repository
# into a release tarball tree
release: klibc.spec
	rm -f maketar.sh .config
