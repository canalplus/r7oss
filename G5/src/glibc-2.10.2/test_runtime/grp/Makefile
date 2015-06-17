# Copyright (C) 1991,1992,1996-2000,2003,2004 Free Software Foundation, Inc.
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
#	Sub-makefile for grp portion of the library.
#

include ../config.make

rtld-installed-name=ld.so

TESTS := testgrp

ifeq (yes,$(build-shared))
TESTS += tst_fgetgrent
endif


# tst_fgetgrent currently only works with shared libraries
ifeq (yes,$(build-shared))
tests: tst_fgetgrent.out
tst_fgetgrent.out: tst_fgetgrent.sh tst_fgetgrent
	$(SHELL) -e tst_fgetgrent.sh ../ ../elf/ $(rtld-installed-name)

endif

include ../Test.mak

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing grp...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
