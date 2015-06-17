# Makefile for csu code for GNU C library.
# Copyright (C) 1995-2004, 2005, 2006 Free Software Foundation, Inc.
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

# This directory contains the C startup code (that which calls main).  This
# consists of the startfile, built from start.c and installed as crt0.o
# (traditionally) or crt1.o (for ELF).	In ELF we also install crti.o and
# crtn.o, special "initializer" and "finalizer" files used in the link
# to make the .init and .fini sections work right; both these files are
# built (in an arcane manner) from initfini.c.

include ../config.make

TESTS := tst-empty tst-atomic tst-atomic-long
tests-static := tst-empty

include ../Test.mak

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
ifneq ("n$(TOBEINSTALLED)n","n n")
	@echo "Installing csu...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
else
	@echo "Nothing to install for csu."
endif
