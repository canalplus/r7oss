# Copyright (C) 1998-2001,2004-2008 Free Software Foundation, Inc.
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
#	Sub-makefile for debug portion of the library.
#

include ../config.make

TESTS = backtrace-tst tst-chk1 tst-chk2 tst-chk3 \
	tst-lfschk1 tst-lfschk2 tst-lfschk3 test-strcpy_chk test-stpcpy_chk \
	tst-chk4 tst-chk5 tst-chk6 tst-lfschk4 tst-lfschk5 tst-lfschk6

WRAPPER_tst-chk1 = LOCPATH=../localedata
WRAPPER_tst-chk2 = LOCPATH=../localedata
WRAPPER_tst-chk3 = LOCPATH=../localedata
WRAPPER_tst-chk4 = LOCPATH=../localedata
WRAPPER_tst-chk5 = LOCPATH=../localedata
WRAPPER_tst-chk6 = LOCPATH=../localedata
WRAPPER_tst-lfschk1 = LOCPATH=../localedata
WRAPPER_tst-lfschk2 = LOCPATH=../localedata
WRAPPER_tst-lfschk3 = LOCPATH=../localedata
WRAPPER_tst-lfschk4 = LOCPATH=../localedata
WRAPPER_tst-lfschk5 = LOCPATH=../localedata
WRAPPER_tst-lfschk6 = LOCPATH=../localedata


others = pcprofiledump
install-bin = pcprofiledump
install-bin-script = xtrace


distribute += catchsegv.sh xtrace.sh
ifeq ($(elf),yes)
ifeq ($(build-shared),yes)
install-bin-script += catchsegv
endif
endif
generated = catchsegv xtrace


../catchsegv: catchsegv.sh $(common-objpfx)soversions.mk \
		    $(common-objpfx)config.make
	slibpfx=`echo $(slibdir)|sed 's/lib\(64\|\)$$/\\\\\\\\$$LIB/'`; \
	sed -e 's|@VERSION@|$(version)|' -e "s|@SLIB@|$$slibpfx|" $< > $@.new
	chmod 555 $@.new
	mv -f $@.new $@

../xtrace: xtrace.sh
	rm -f $@.new
	sed -e 's|@BASH@|$(BASH)|' -e 's|@VERSION@|$(version)|' \
	    -e 's|@SLIBDIR@|$(sLIBdir)|' -e 's|@BINDIR@|$(bindir)|' $^ > $@.new \
	&& rm -f $@ && mv $@.new $@ && chmod +x $@

include ../Test.mak

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing debug...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
