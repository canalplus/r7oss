# Copyright (C) 1991-2006, 2007, 2008, 2009 Free Software Foundation, Inc.
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
#	Specific makefile for stdio-common.
#

include ../config.make

TESTS := tstscanf test_rdwr test-popen tstgetln test-fseek \
	 temptest tst-fileno test-fwrite tst-ungetc tst-ferror \
	 xbug errnobug \
	 bug1 bug2 bug3 bug4 bug5 bug6 bug7 bug8 bug9 bug10 bug11 bug12 bug13 \
	 tfformat tiformat tllformat tstdiomisc tst-printfsz tst-wc-printf \
	 scanf1 scanf2 scanf3 scanf4 scanf5 scanf7 scanf8 scanf9 scanf10 \
	 scanf11 scanf12 tst-tmpnam tst-cookie tst-obprintf tst-sscanf \
	 tst-swprintf tst-fseek tst-fmemopen test-vfprintf tst-gets \
	 tst-perror tst-sprintf tst-rndseek tst-fdopen tst-fphex bug14 bug15 \
	 tst-popen tst-unlockedio tst-fmemopen2 tst-put-error tst-fgets \
	 tst-fwrite bug16 bug17 tst-swscanf tst-sprintf2 bug18 bug18a \
	 bug19 bug19a tst-popen2 scanf13 scanf14 scanf15 bug20 bug21 bug22 \
	 scanf16 scanf17 tst-setvbuf1

# the following seem to hang :
TESTS_DISABLED=tstscanf tstgetln temptest tst-ferror bug6 \
		scanf12 tst-gets bug22

PREBUILT_TESTS+= tst-unbputc tst-printf

tests: renew_prebuilt_tests \
	$(addsuffix .out, $(PREBUILT_TESTS))

renew_prebuilt_tests:
	@touch $(PREBUILT_TESTS)

tst-unbputc.out: tst-unbputc.sh tst-unbputc
	$(SHELL) -e $< ../ './tst-unbputc'

tst-printf.out: tst-printf.sh tst-printf
	$(SHELL) -e $< ../ './tst-printf'

WRAPPER_tst-sscanf = LOCPATH=../localedata
WRAPPER_tst-swprintf = LOCPATH=../localedata
WRAPPER_test-vfprintf = LOCPATH=../localedata
WRAPPER_scanf13 = LOCPATH=../localedata
WRAPPER_bug14 = LOCPATH=../localedata
WRAPPER_bug15 = LOCPATH=../localedata

include ../Test.mak

tst-setvbuf1.out: tst-setvbuf1.expect tst-setvbuf1
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@./tst-setvbuf1 > $@ 2>&1
	@cmp tst-setvbuf1.expect $@

TOBEINSTALLED_OTHERS=../stdio-common/*.expect \
                     ../stdio-common/*.input

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing stdio-common...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
