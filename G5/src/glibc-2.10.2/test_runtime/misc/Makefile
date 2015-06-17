# Copyright (C) 1991-2006, 2007, 2009 Free Software Foundation, Inc.
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
#	Sub-makefile for misc portion of the library.
#

include ../config.make

TESTS := tst-dirname tst-tsearch tst-fdset tst-efgcvt tst-mntent tst-hsearch \
	 tst-error1 tst-pselect tst-insremque tst-mntent2 bug-hsearch1


tests: $(objpfx)tst-error1-mem

WRAPPER_tst-error1 = MALLOC_TRACE=$(objpfx)tst-error1.mtrace
OPTS_tst-error1 = $(objpfx)tst-error1.out
$(objpfx)tst-error1-mem: $(objpfx)tst-error1.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-error1.mtrace > $@

include ../Test.mak

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
ifneq ("n$(TOBEINSTALLED)n","n n")
	@echo "Installing misc...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
else
	@echo "Nothing to install for misc."
endif
