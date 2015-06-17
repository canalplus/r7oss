# Copyright (C) 1995-2003, 2005, 2008 Free Software Foundation, Inc.
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

# Makefile for intl subdirectory: message handling code from GNU gettext.

include ../config.make

TESTS = tst-ngettext

include ../Test.mak

ifeq (yes,$(build-shared))
ifneq ($(strip $(MSGFMT)),:)
tests: tst-translit.out tst-gettext2.out \
       tst-codeset.out tst-gettext3.out
ifeq ($(have-thread-library),yes)
tests: tst-gettext4.out tst-gettext5.out \
       tst-gettext6.out
endif
ifneq (no,$(PERL))
tests: mtrace-tst-gettext
endif
endif
mtrace-tst-gettext: tst-gettext.out
	../malloc/mtrace $(objpfx)tst-gettext.mtrace > $@
tst-gettext.out: tst-gettext.sh $(objpfx)tst-gettext
	$(SHELL) -e $<  ../intl/ \
	  $(objpfx)tst-gettext.mtrace
tst-translit.out: tst-translit.sh tst-translit
	$(SHELL) -e $< ../ ../intl/
tst-gettext2.out: tst-gettext2.sh tst-gettext2
	$(SHELL) -e $< ../ ../intl/
tst-codeset.out: tst-codeset.sh tst-codeset
	$(SHELL) -e $< ../ ../intl/
tst-gettext3.out: tst-gettext3.sh tst-gettext3
	$(SHELL) -e $< ../ ../intl/
tst-gettext4.out: tst-gettext4.sh tst-gettext4
	$(SHELL) -e $< ../ '$(run-program-prefix)' ../intl/
tst-gettext5.out: tst-gettext5.sh tst-gettext5
	$(SHELL) -e $< ../ '$(run-program-prefix)' ../intl/
tst-gettext6.out: tst-gettext6.sh tst-gettext6
	$(SHELL) -e $< ../ '$(run-program-prefix)' ../intl/
endif

tst-translit.out: tst-gettext.out
tst-gettext2.out: tst-gettext.out
tst-codeset.out: tst-gettext.out
tst-gettext3.out: tst-gettext.out
tst-gettext4.out: tst-gettext.out
tst-gettext5.out: tst-gettext.out
tst-gettext6.out: tst-gettext.out

TOBEINSTALLED_OTHERS=intl/msgs.h

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing intl....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
