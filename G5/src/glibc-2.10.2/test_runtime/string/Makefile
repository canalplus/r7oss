# Copyright (C) 1991-2002, 2005-2008, 2009 Free Software Foundation, Inc.
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
#	Sub-makefile for string portion of library.
#

include ../config.make

strop-tests	:= memchr memcmp memcpy memmove mempcpy memset memccpy	\
		   stpcpy stpncpy strcat strchr strcmp strcpy strcspn	\
		   strlen strncmp strncpy strpbrk strrchr strspn memmem

TESTS		:= tester inl-tester noinl-tester testcopy test-ffs	\
		   tst-strlen stratcliff tst-svc tst-inlcall		\
		   bug-strncat1 bug-strspn1 bug-strpbrk1 tst-bswap	\
		   tst-strtok tst-strxfrm bug-strcoll1 tst-strfry	\
		   bug-strtok1 $(addprefix test-,$(strop-tests))	\
		   bug-envz1 tst-strxfrm2 tst-endian tst-svc2

WRAPPER_tester = LANGUAGE=C
WRAPPER_inl-tester = LANGUAGE=C
WRAPPER_noinl-tester = LANGUAGE=C
WRAPPER_tst-strxfrm = LOCPATH=../localedata
WRAPPER_tst-strxfrm2 = LOCPATH=../localedata
WRAPPER_bug-strcoll1 = LOCPATH=../localedata

tst-svc.out: tst-svc.input tst-svc
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@\
	GCONV_PATH=../iconvdata LC_ALL=C \
		./tst-svc < $(word 1,$^) > $@ 2>&1 ; \
	cmp tst-svc.expect $@ > /dev/null 2>&1 ; \
	ret=$$? ; \
	if ! test $$ret -eq 0 ; then \
		echo "ret == $$ret ; expected_ret == 0" ; \
		exit 1 ; \
	fi
	-@true "$@"

include ../Test.mak

TOBEINSTALLED_OTHERS=../string/*.expect \
                     ../string/*.input

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing string...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
