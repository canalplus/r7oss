# Copyright (C) 1997-2004, 2006, 2007 Free Software Foundation, Inc.
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
#	Sub-makefile for real-time portion of the library.
#

include ../config.make

TESTS := tst-shm tst-clock tst-clock_nanosleep tst-timer tst-timer2 \
	 tst-aio tst-aio64 tst-aio2 tst-aio3 tst-aio4 tst-aio5 tst-aio6 \
	 tst-aio7 tst-aio8 tst-aio9 tst-aio10 \
	 tst-mqueue1 tst-mqueue2 tst-mqueue3 tst-mqueue4 \
	 tst-mqueue5 tst-mqueue6 tst-mqueue7 tst-mqueue8 tst-mqueue9 \
	 tst-timer3 tst-timer4 tst-timer5 \
	 tst-cpuclock1 tst-cpuclock2 \
	 tst-cputimer1 tst-cputimer2 tst-cputimer3 \
	 tst-clock2

distribute := aio_misc.h

OPTS_tst-mqueue7 = -- $(built-program-cmd)

include ../Test.mak

TOBEINSTALLED_OTHERS= rt/librt.so

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing rt....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
