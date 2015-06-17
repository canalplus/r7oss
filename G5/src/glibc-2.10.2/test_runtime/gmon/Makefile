# Copyright (C) 1995,1996,1997,2001,2002,2005 Free Software Foundation, Inc.
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
#	Sub-makefile for gmon portion of the library.
#

include ../config.make

TESTS	= tst-sprofil
ifeq ($(build-profile),yes)
TESTS	+= tst-profile-static
#tests-static	+= tst-profile-static
endif

include ../Test.mak

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
ifneq ("n$(TOBEINSTALLED)n","n n")
	@echo "Installing gmon...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
else
	@echo "Nothing to install for gmon."
endif
