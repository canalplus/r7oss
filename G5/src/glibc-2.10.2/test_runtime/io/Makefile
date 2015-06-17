# Copyright (C) 1992-2003,2005,2006,2007,2008 Free Software Foundation, Inc.
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
#	Sub-makefile for I/O portion of the library.
#

include ../config.make

TESTS		:= test-utime test-stat test-stat2 test-lfs tst-getcwd \
		   tst-fcntl bug-ftw1 bug-ftw2 bug-ftw3 bug-ftw4 tst-statvfs \
		   tst-openat tst-unlinkat tst-fstatat tst-futimesat \
		   tst-renameat tst-fchownat tst-fchmodat tst-faccessat \
		   tst-symlinkat tst-linkat tst-readlinkat tst-mkdirat \
		   tst-mknodat tst-mkfifoat tst-ttyname_r bug-ftw5 \
		   tst-posix_fallocate

OPTS_test-stat2 = Makefile . test-stat2

OPTS_tst-statvfs = tst-statvfs tst-statvfs.c /tmp

tests: ftwtest.out

ftwtest.out: ftwtest
	$(SHELL) -e ftwtest-sh $(shell cd .. && pwd)/ \
		 $(shell cd $(<D) && pwd)/$(<F) > $@

include ../Test.mak

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing io...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
