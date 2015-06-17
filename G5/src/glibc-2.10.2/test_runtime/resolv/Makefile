# Copyright (C) 1994-2001,2003,2004,2007,2008 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA.

#
#	Sub-makefile for resolv portion of the library.
#
include ../config.make

TESTS = tst-aton tst-leaks tst-inet_ntop
xtests = tst-leaks2

include ../Test.mak

generate := mtrace-tst-leaks tst-leaks.mtrace tst-leaks2.mtrace

subdir-dirs = nss_dns
vpath %.c nss_dns

libnss_dns-routines	:= dns-host dns-network dns-canon
ifneq ($(build-static-nss),yes)
libnss_dns-inhibit-o	= $(filter-out .os,$(object-suffixes))
endif

ifeq (yesyes,$(build-shared)$(have-thread-library))
tests: $(objpfx)ga_test
endif

generated := mtrace-tst-leaks tst-leaks.mtrace \
	     mtrace-tst-leaks2 tst-leaks2.mtrace

$(objpfx)mtrace-tst-leaks: $(objpfx)tst-leaks.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-leaks.mtrace > $@
ifneq (no,$(PERL))
tests: $(objpfx)mtrace-tst-leaks
endif

WRAPPER_tst-leaks2 = MALLOC_TRACE=$(objpfx)tst-leaks2.mtrace
$(objpfx)mtrace-tst-leaks2: $(objpfx)tst-leaks2.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-leaks2.mtrace > $@
ifneq (no,$(PERL))
xtests: $(objpfx)mtrace-tst-leaks2
endif

TOBEINSTALLED_OTHERS=resolv/libresolv.so

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing resolv....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
