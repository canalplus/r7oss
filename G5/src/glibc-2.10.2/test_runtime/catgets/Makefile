# Copyright (C) 1996-2000, 2002, 2003, 2004 Free Software Foundation, Inc.
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
#	Makefile for catgets.
#

include ../config.make

.PHONY: clean tests

TESTS = tst-catgets

WRAPPER_tst-catgets = NLSPATH="./%l/%N.cat" LANG=de

test-input-data= de/libc.cat test1.cat test2.cat

PREBUILT_TESTS= test-gencat

tests: $(test-input-data) renew_prebuilt_tests \
	$(addsuffix .out, $(PREBUILT_TESTS))

renew_prebuilt_tests:
	@touch $(PREBUILT_TESTS)

# This test just checks whether the program produces any error or not.
# The result is not tested.
test1.cat: test1.msg gencat
	@LC_ALL=hr_HR.ISO-8859-2 LOCPATH=../localedata \
	GCONV_PATH=../iconvdata \
	gencat -H test1.h $@ $<

test2.cat: test2.msg gencat
	@LOCPATH=../localedata \
	GCONV_PATH=../iconvdata \
	gencat -H test2.h $@ $<

de/libc.cat: de.msg gencat
	@mkdir -p de
	@LC_ALL=de_DE.ISO-8859-1 LOCPATH=../localedata \
	GCONV_PATH=../iconvdata gencat $@ $< > $@.out 2>&1 && \
	touch $@.ret || true

tst-catgets.out: de/libc.cat
	@cat $<.out>  $@ &&  rm -f $<.out ; \
	echo "  TEST_EXEC $(notdir $(CURDIR))/ $*" ; \
	if ! test -f $<.ret ; then \
		exit 1 ; \
	else \
		rm -f $<.ret ; \
	fi
	-@true "$@"

# Generate a non-simple input file.
de.msg: ../po/de.po
	@LC_ALL=C sed -f xopen-msg.sed $< > $@

test-gencat.out: test-gencat.sh test-gencat \
			  sample.SJIS.cat
	$(SHELL) -e $< ../ './test-gencat'

sample.SJIS.cat: sample.SJIS gencat
	GCONV_PATH=../iconvdata LC_ALL=C \
	gencat -H test-gencat.h < $(word 1,$^) > $@

EXTRA_CLEAN= sample.SJIS.cat tst-catgets.out  \
	     test-gencat.out *.cat de/*.cat de.msg

include ../Test.mak

TOBEINSTALLED_OTHERS=../catgets/xopen-msg.sed \
	            ../catgets/test*.msg \
	            ../catgets/sample.SJIS

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing catgets....."
	cp -fr $(foreach f,$^, $(OBJPFX)/$f) .

