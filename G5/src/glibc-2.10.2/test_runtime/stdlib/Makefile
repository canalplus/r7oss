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
#	Makefile for stdlib routines
#

include ../config.make

test-srcs	:= tst-fmtmsg
TESTS		:= tst-strtol tst-strtod testmb testrand testsort testdiv   \
		   test-canon test-canon2 tst-strtoll tst-environ	    \
		   tst-xpg-basename tst-random tst-random2 tst-bsearch	    \
		   tst-limits tst-rand48 bug-strtod tst-setcontext	    \
		   test-a64l tst-qsort tst-system testmb2 bug-strtod2	    \
		   tst-atof1 tst-atof2 tst-strtod2 tst-strtod3 tst-rand48-2 \
		   tst-makecontext tst-strtod4 tst-strtod5 tst-qsort2	    \
		   tst-makecontext2 tst-strtod6 tst-unsetenv1

TESTS_DISABLED=testdiv tst-qsort2

ifeq ($(build-shared),yes)
TESTS		+= tst-putenv
endif

distribute := $(distribute) $(mpn-headers) fpioconst.h tst-putenvmod.c

tests-extras += tst-putenvmod

generated += isomac isomac.out tst-putenvmod.so

# Testdir has to be named stdlib and needs to be writable
OPTS_test-canon = --test-dir=${common-objpfx}stdlib

WRAPPER_tst-strtod = LOCPATH=../localedata
WRAPPER_tst-strtod3 = LOCPATH=../localedata
WRAPPER_tst-strtod4 = LOCPATH=../localedata
WRAPPER_tst-strtod5 = LOCPATH=../localedata
WRAPPER_testmb2 = LOCPATH=../localedata

# Run a test on the header files we use.

$(objpfx)isomac.out: $(objpfx)isomac
	$(dir $<)$(notdir $<) '$(CC)'  \
	'-I../include $(+sysdep-includes) $(sysincludes) -I..' > $<.out

isomac-CFLAGS = -O

$(objpfx)tst-fmtmsg.out: tst-fmtmsg.sh $(objpfx)tst-fmtmsg
	$(SHELL) -e $< $(common-objpfx) '$(run-program-prefix)' $(common-objpfx)stdlib/

include ../Test.mak

TOBEINSTALLED_OTHERS=stdlib/*.so \
                     ../stdlib/*.input

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing stdlib...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
