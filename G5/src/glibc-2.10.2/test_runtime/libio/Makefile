# Copyright (C) 1995-2004,2006,2007,2008,2009 Free Software Foundation, Inc.
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
#	Specific makefile for libio.
#

include ../config.make

TESTS = tst_swprintf tst_wprintf tst_swscanf tst_wscanf tst_getwc tst_putwc   \
	tst_wprintf2 tst-widetext test-fmemopen tst-ext tst-ext2 tst-fopenloc \
	tst-fgetws tst-ungetwc1 tst-ungetwc2 tst-swscanf tst-sscanf	      \
	tst-mmap-setvbuf bug-ungetwc1 bug-ungetwc2 tst-atime tst-eof          \
	tst-freopen bug-rewind bug-rewind2 bug-ungetc bug-fseek \
	tst-mmap-eofsync tst-mmap-fflushsync bug-mmap-fflush \
	tst-mmap2-eofsync tst-mmap-offend bug-fopena+ bug-wfflush \
	bug-ungetc2 bug-ftell bug-ungetc3 bug-ungetc4 tst-fopenloc2 \
	tst-memstream1 tst-memstream2 \
	tst-wmemstream1 tst-wmemstream2 \
	bug-memstream1 bug-wmemstream1 \
	tst-setvbuf1 tst-popen1 tst-fgetwc

TESTS_DISABLED=tst_wscanf tst_getwc tst-widetext

OPTS_tst_wprintf2 = "Some Text"

WRAPPER_tst-widetext = LOCPATH=../localedata LANGUAGE=C
WRAPPER_tst-fopenloc = LOCPATH=../localedata MALLOC_TRACE=enloc.mtrace
WRAPPER_tst-fgetws = LOCPATH=../localedata
WRAPPER_tst-ungetwc1 = LOCPATH=../localedata
WRAPPER_tst-ungetwc2 = LOCPATH=../localedata
WRAPPER_bug-ungetwc2 = LOCPATH=../localedata
WRAPPER_tst-swscanf = LOCPATH=../localedata
WRAPPER_bug-ftell = LOCPATH=../localedata

PREBUILT_TESTS+= test-freopen tst-fopenloc.check

tests: renew_prebuilt_tests \
	$(addsuffix .out, $(PREBUILT_TESTS))

renew_prebuilt_tests:
	@touch $(PREBUILT_TESTS)

test-freopen.out: test-freopen.sh test-freopen
	$(SHELL) -e $< ../ '$(run-program-prefix)'	\
	../libio/

tst-fopenloc.check: tst-fopenloc.out
	cmp ../iconvdata/testdata/ISO-8859-1..UTF8 tst-fopenloc.out \
	  > $@
	../malloc/mtrace tst-fopenloc.mtrace >> $@

include ../Test.mak

TOBEINSTALLED_OTHERS=../libio/*.input

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing libio...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
