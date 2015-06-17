# Copyright (C) 1996, 2000, 2001, 2007, 2009 Free Software Foundation, Inc.
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
#	Sub-makefile for crypt() portion of the library.
#

include ../config.make

TESTS := cert md5c-test sha256c-test sha512c-test
TESTS_DISABLED=cert # takes very long, maybe hangs ?
ifneq ($(nss-crypt),yes)
TESTS += md5test sha256test sha512test
endif

include ../Test.mak

TOBEINSTALLED_OTHERS=crypt/libcrypt.so \
                     ../crypt/*.input

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing crypt....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
