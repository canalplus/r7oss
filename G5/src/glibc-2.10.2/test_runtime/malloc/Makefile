# Copyright (C) 1991-1999, 2000, 2001, 2002, 2003, 2005, 2006, 2007, 2009
# Free Software Foundation, Inc.
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
#	Makefile for malloc routines
#

include ../config.make

TESTS := mallocbug tst-malloc tst-valloc tst-calloc tst-obstack \
	 tst-mallocstate tst-mcheck tst-mallocfork tst-trim1
#test-srcs = tst-mtrace

WRAPPER_tst-mcheck = MALLOC_CHECK_=3

sLIBdir := $(shell echo $(slibdir) | sed 's,lib\(\|64\)$$,\\\\$$LIB,')

include ../Test.mak

TOBEINSTALLED=$(filter-out malloc/mtrace,$(TOBEINSTALLED_BIN))

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	echo "n$(TOBEINSTALLED)n"
ifneq ("n$(TOBEINSTALLED)n","nn")
	@echo "Installing malloc....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
else
	@echo "Nothing to install for malloc."
endif
