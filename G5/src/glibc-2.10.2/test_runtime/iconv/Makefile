# Copyright (C) 1997,1998,2000-2004,2007,2009 Free Software Foundation, Inc.
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
#	Makefile for iconv.
#
#

include ../config.make

TESTS	= tst-iconv1 tst-iconv2 tst-iconv3 tst-iconv4 tst-iconv5

xtests: test-iconvconfig

include ../Test.mak

.PHONY: test-iconvconfig
test-iconvconfig: /dev/null iconvconfig
	tmp=gconv-modules.cache.$$$$; \
	rm -f $$tmp; \
	GCONV_PATH=../iconvdata LC_ALL=C $(WRAPPER_$*) iconvconfig $(OPTS_$*) --output=$$tmp --nostdlib /usr/local/lib/gconv && \
	cmp $$tmp /usr/local/lib/gconv/gconv-modules.cache && \
	rm -f $$tmp

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing iconv...."
	@mkdir -p $(PFX)/usr/local/bin
	@cp $(OBJPFX)/iconv/iconvconfig $(PFX)/usr/local/bin/iconv
	@cp $(OBJPFX)/iconv/iconv_prog $(PFX)/usr/local/bin
	@ln -s -f iconv $(PFX)/usr/local/bin/iconv_prog
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
