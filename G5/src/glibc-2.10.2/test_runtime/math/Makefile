# Copyright (C) 1996-2001,2002,2003,2004,2005,2006
#	Free Software Foundation, Inc.
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

# Makefile for the math library.

# Rules for the test suite.

include ../config.make

TESTS = test-matherr test-fenv atest-exp atest-sincos atest-exp2 basic-test \
	test-misc test-fpucw tst-definitions test-tgmath test-tgmath-ret \
	bug-nextafter bug-nexttoward bug-tgmath1 test-tgmath-int

TESTS_DISABLED=test-tgmath2

# We do the `long double' tests only if this data type is available and
# distinct from `double'.
test-longdouble-yes = test-ldouble test-ildoubl
distribute += $(test-longdouble-yes:=.c)

ifneq (,$(PERL))
libm-tests = test-float test-double $(test-longdouble-$(long-double-fcts)) \
	test-ifloat test-idouble

TESTS += $(libm-tests)

endif

include ../Test.mak

TOBEINSTALLED_OTHERS=math/libm.so

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing math....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
