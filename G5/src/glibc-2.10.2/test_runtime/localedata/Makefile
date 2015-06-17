# Copyright (C) 1996-2003,2005,2007,2008,2009 Free Software Foundation, Inc.
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

# Makefile for installing locale data source files.

include ../config.make

# List with all available character set descriptions.
charmaps := $(filter-out $(addprefix charmaps/, CVS RCS SCCS %~), \
				     $(wildcard charmaps/[A-I]*) \
				     $(wildcard charmaps/[J-Z]*))

# List with all available character set descriptions.
locales := $(filter-out $(addprefix locales/, CVS RCS SCCS %~), \
				    $(wildcard locales/*))


subdir-dirs = tests-mbwc
vpath %.c tests-mbwc
vpath %.h tests-mbwc


test-srcs := collate-test xfrm-test tst-fmon tst-rpmatch tst-trans \
	     tst-mbswcs1 tst-mbswcs2 tst-mbswcs3 tst-mbswcs4 tst-mbswcs5 \
	     tst-ctype tst-wctype tst-langinfo tst-numeric
test-input := de_DE.ISO-8859-1 en_US.ISO-8859-1 da_DK.ISO-8859-1 \
	      hr_HR.ISO-8859-2 sv_SE.ISO-8859-1 tr_TR.UTF-8 fr_FR.UTF-8 \
	      si_LK.UTF-8

test-input-data = $(addsuffix .in, $(basename $(test-input)))
test-output := $(foreach s, .out .xout, \
			 $(addsuffix $s, $(basename $(test-input))))
ld-test-names := test1 test2 test3 test4 test5 test6 test7
ld-test-srcs := $(addprefix tests/,$(addsuffix .cm,$(ld-test-names)) \
				   $(addsuffix .def,$(ld-test-names)) \
				   $(addsuffix .ds,test5 test6) \
				   test6.c trans.def)

fmon-tests = n01y12 n02n40 n10y31 n11y41 n12y11 n20n32 n30y20 n41n00 \
	     y01y10 y02n22 y22n42 y30y21 y32n31 y40y00 y42n21

generated := $(test-input) $(test-output) sort-test.out tst-locale.out \
	     tst-mbswcs.out tst-leaks.mtrace mtrace-tst-leaks
generated-dirs := $(ld-test-names) tt_TT de_DE.437			\
		  $(addprefix tstfmon_,$(fmon-tests))			\

distribute := CHECKSUMS README SUPPORTED ChangeLog			\
	      $(charmaps) $(locales)					\
	      tst-rpmatch.sh tst-locale.sh tst-fmon.sh sort-test.sh	\
	      tst-fmon.data $(test-input-data) $(ld-test-srcs)		\
	      th_TH.in cs_CZ.in tst-mbswcs.sh tst-trans.sh tst-ctype.sh \
	      tst-ctype-de_DE.ISO-8859-1.in				\
	      tst-numeric.sh tst-numeric.data				\
	      $(wildcard tests-mbwc/*.[ch])				\
	      $(addprefix tst-fmon-locales/tstfmon_,$(fmon-tests))	\
	      gen-locale.sh show-ucs-data.c tst-langinfo.sh		\
	      tst-wctype.sh tst-wctype.input gen-unicode-ctype.c	\
	      dump-ctype.c

locale_test_suite := tst_iswalnum tst_iswalpha tst_iswcntrl            \
		     tst_iswctype tst_iswdigit tst_iswgraph            \
		     tst_iswlower tst_iswprint tst_iswpunct            \
		     tst_iswspace tst_iswupper tst_iswxdigit tst_mblen \
		     tst_mbrlen tst_mbrtowc tst_mbsrtowcs tst_mbstowcs \
		     tst_mbtowc tst_strcoll tst_strfmon tst_strxfrm    \
		     tst_swscanf tst_towctrans tst_towlower            \
		     tst_towupper tst_wcrtomb tst_wcscat tst_wcschr    \
		     tst_wcscmp tst_wcscoll tst_wcscpy tst_wcscspn     \
		     tst_wcslen tst_wcsncat tst_wcsncmp tst_wcsncpy    \
		     tst_wcspbrk tst_wcsrtombs tst_wcsspn tst_wcsstr   \
		     tst_wcstod tst_wcstok tst_wcstombs tst_wcswidth   \
		     tst_wcsxfrm tst_wctob tst_wctomb tst_wctrans      \
		     tst_wctype tst_wcwidth

# SC - currently skipped, FIXME
#TESTS = $(locale_test_suite) tst-digits tst-setlocale bug-iconv-trans
TESTS = tst-digits tst-setlocale bug-iconv-trans \
	tst-leaks tst-mbswcs6 tst-xlocale1 tst-xlocale2 bug-usesetlocale \
	tst-strfmon1 tst-sscanf tst-strptime
ifeq (yes,$(build-shared))
ifneq (no,$(PERL))
TESTS: mtrace-tst-leaks
endif
endif
tests:$(TESTS)

# Files to install.
install-others := $(addprefix $(inst_i18ndir)/, \
			      $(addsuffix .gz, $(charmaps)) \
			      $(locales))

# Install the charmap files in gzipped format.
$(inst_i18ndir)/charmaps/%.gz: charmaps/% $(+force)
	$(make-target-directory)
	rm -f $(@:.gz=) $@
	$(INSTALL_DATA) $< $(@:.gz=)
	gzip -9n $(@:.gz=)

# Install the locale source files in the appropriate directory.
$(inst_i18ndir)/locales/%: locales/% $(+force); $(do-install)

# gcc does not know all the format specifiers we are using here.
CFLAGS-tst-mbswcs1.c = -Wno-format
CFLAGS-tst-mbswcs2.c = -Wno-format
CFLAGS-tst-mbswcs3.c = -Wno-format
CFLAGS-tst-mbswcs4.c = -Wno-format
CFLAGS-tst-mbswcs5.c = -Wno-format
CFLAGS-tst-trans.c = -Wno-format
CFLAGS += -I.. -D_GNU_SOURCE
tst-xlocale1.c-CFLAGS += -D_GNU_SOURCE
CFLAGS-tst-xlocale1.c += -D_GNU_SOURCE
CFLAGS-tst-xlocale2.c = -D_GNU_SOURCE
CFLAGS-bug-usesetlocale.c = -D_GNU_SOURCE
CFLAGS-tst-strfmon1.c = -D_GNU_SOURCE
CFLAGS-tst-sscanf.c = -D_GNU_SOURCE
CFLAGS-tst-strptime.c = -D_GNU_SOURCE

# We have to generate locales
LOCALES := de_DE.ISO-8859-1 de_DE.UTF-8 en_US.ANSI_X3.4-1968 \
	   en_US.ISO-8859-1 ja_JP.EUC-JP da_DK.ISO-8859-1 \
	   hr_HR.ISO-8859-2 sv_SE.ISO-8859-1 ja_JP.SJIS fr_FR.ISO-8859-1 \
	   vi_VN.TCVN5712-1 nb_NO.ISO-8859-1 nn_NO.ISO-8859-1 \
	   tr_TR.UTF-8 cs_CZ.UTF-8 zh_TW.EUC-TW fa_IR.UTF-8 fr_FR.UTF-8 \
	   ja_JP.UTF-8 si_LK.UTF-8
LOCALE_SRCS := $(shell echo "$(LOCALES)"|sed 's/\([^ .]*\)[^ ]*/\1/g')
CHARMAPS := $(shell echo "$(LOCALES)" | \
		    sed -e 's/[^ .]*[.]\([^ ]*\)/\1/g' -e s/SJIS/SHIFT_JIS/g)
CTYPE_FILES = $(addsuffix /LC_CTYPE,$(LOCALES))

generated-dirs += $(LOCALES)

# Dependency for the locale files.  We actually make it depend only on
# one of the files.
$(CTYPE_FILES): %: \
  gen-locale.sh ../locale/localedef Makefile \
  $(addprefix charmaps/,$(CHARMAPS)) $(addprefix locales/,$(LOCALE_SRCS))
	@$(SHELL) -e gen-locale.sh ../ '../locale/localedef' $@

$(addsuffix .out,$(TESTS)): %: $(CTYPE_FILES)

PREBUILT_TESTS= sort-test tst-fmon tst-locale \
       tst-rpmatch tst-trans tst-mbswcs tst-ctype \
       tst-wctype tst-langinfo tst-numeric

tests: $(test-input-data) $(CTYPE_FILES) \
	renew_prebuilt_tests \
	$(addsuffix .out, $(PREBUILT_TESTS))

renew_prebuilt_tests:
	@touch $(PREBUILT_TESTS)

sort-test.out: sort-test.sh collate-test xfrm-test
	$(SHELL) -e $< ../ './collate-test' $(test-input) > $@
tst-fmon.out: tst-fmon.sh tst-fmon tst-fmon.data \
		       sort-test.out
	$(SHELL) -e $< ../ '' tst-fmon.data > $@
tst-numeric.out: tst-numeric.sh tst-numeric tst-numeric.data \
		       sort-test.out
	$(SHELL) -e $< ../ '' tst-numeric.data > $@
tst-locale.out: tst-locale.sh ../locale/localedef \
			 $(ld-test-srcs)
	$(SHELL) -e $< ../ '../locale/localedef' > $@
tst-rpmatch.out: tst-rpmatch.sh tst-rpmatch \
			  tst-fmon.out
	$(SHELL) -e $< ../ './tst-rpmatch' > $@
tst-trans.out: tst-trans.sh tst-trans $(CTYPE_FILES)
	$(SHELL) -e $< ../ ''
tst-mbswcs.out: tst-mbswcs.sh tst-mbswcs1 \
			 tst-mbswcs2 tst-mbswcs3 \
			 tst-mbswcs4 tst-mbswcs5
	$(SHELL) -e $< ../ ''
tst-ctype.out: tst-ctype.sh tst-ctype \
			sort-test.out
	$(SHELL) -e $< ../ './tst-ctype'
tst-wctype.out: tst-wctype.sh tst-wctype \
			sort-test.out tst-wctype.input
	$(SHELL) -e $< ../ ''
tst-langinfo.out: tst-langinfo.sh tst-langinfo \
			sort-test.out
	$(SHELL) -e $< ../ ''
tst-digits.out: tst-locale.out
tst-mbswcs6.out: $(CTYPE_FILES)

ifneq ($(MAKECMDGOALS),install)
include SUPPORTED
endif

INSTALL-SUPPORTED-LOCALES=$(addprefix install-, $(SUPPORTED-LOCALES))

# Sometimes the whole collection of locale files should be installed.
LOCALEDEF=I18NPATH=. GCONV_PATH=../iconvdata LC_ALL=C \
../elf/ld.so --library-path $(rpath-link) ../locale/localedef
install-locales: $(INSTALL-SUPPORTED-LOCALES)

install-locales-dir:
	$(..)./scripts/mkinstalldirs $(inst_localedir)

$(INSTALL-SUPPORTED-LOCALES): install-locales-dir
	@locale=`echo $@ | sed -e 's/^install-//'`; \
	charset=`echo $$locale | sed -e 's,.*/,,'`; \
	locale=`echo $$locale | sed -e 's,/[^/]*,,'`; \
	echo -n `echo $$locale | sed 's/\([^.\@]*\).*/\1/'`; \
	echo -n ".$$charset"; \
	echo -n `echo $$locale | sed 's/\([^\@]*\)\(\@.*\)*/\2/'`; \
	echo -n '...'; \
	input=`echo $$locale | sed 's/\([^.]*\)[^@]*\(.*\)/\1\2/'`; \
	$(LOCALEDEF) --alias-file=../intl/locale.alias \
		     -i locales/$$input -c -f charmaps/$$charset \
		     $(addprefix --prefix=,$(install_root)) $$locale; \
	echo ' done'; \

# The mbwc-tests need some environment setup to find the locale data files
TEST_MBWC_ENV:= LOCPATH=../localedata

WRAPPER_tst_iswalnum = $(TEST_MBWC_ENV)
WRAPPER_tst_iswalpha = $(TEST_MBWC_ENV)
WRAPPER_tst_iswcntrl = $(TEST_MBWC_ENV)
WRAPPER_tst_iswctype = $(TEST_MBWC_ENV)
WRAPPER_tst_iswdigit = $(TEST_MBWC_ENV)
WRAPPER_tst_iswgraph = $(TEST_MBWC_ENV)
WRAPPER_tst_iswlower = $(TEST_MBWC_ENV)
WRAPPER_tst_iswprint = $(TEST_MBWC_ENV)
WRAPPER_tst_iswpunct = $(TEST_MBWC_ENV)
WRAPPER_tst_iswspace = $(TEST_MBWC_ENV)
WRAPPER_tst_iswupper = $(TEST_MBWC_ENV)
WRAPPER_tst_iswxdigit = $(TEST_MBWC_ENV)
WRAPPER_tst_mblen = $(TEST_MBWC_ENV)
WRAPPER_tst_mbrlen = $(TEST_MBWC_ENV)
WRAPPER_tst_mbrtowc = $(TEST_MBWC_ENV)
WRAPPER_tst_mbsrtowcs = $(TEST_MBWC_ENV)
WRAPPER_tst_mbstowcs = $(TEST_MBWC_ENV)
WRAPPER_tst_mbtowc = $(TEST_MBWC_ENV)
WRAPPER_tst_strcoll = $(TEST_MBWC_ENV)
WRAPPER_tst_strfmon = $(TEST_MBWC_ENV)
WRAPPER_tst_strxfrm = $(TEST_MBWC_ENV)
WRAPPER_tst_swscanf = $(TEST_MBWC_ENV)
WRAPPER_tst_towctrans = $(TEST_MBWC_ENV)
WRAPPER_tst_towlower = $(TEST_MBWC_ENV)
WRAPPER_tst_towupper = $(TEST_MBWC_ENV)
WRAPPER_tst_wcrtomb = $(TEST_MBWC_ENV)
WRAPPER_tst_wcscat = $(TEST_MBWC_ENV)
WRAPPER_tst_wcschr = $(TEST_MBWC_ENV)
WRAPPER_tst_wcscmp = $(TEST_MBWC_ENV)
WRAPPER_tst_wcscoll = $(TEST_MBWC_ENV)
WRAPPER_tst_wcscpy = $(TEST_MBWC_ENV)
WRAPPER_tst_wcscspn = $(TEST_MBWC_ENV)
WRAPPER_tst_wcslen = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsncat = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsncmp = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsncpy = $(TEST_MBWC_ENV)
WRAPPER_tst_wcspbrk = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsrtombs = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsspn = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsstr = $(TEST_MBWC_ENV)
WRAPPER_tst_wcstod = $(TEST_MBWC_ENV)
WRAPPER_tst_wcstok = $(TEST_MBWC_ENV)
WRAPPER_tst_wcstombs = $(TEST_MBWC_ENV)
WRAPPER_tst_wcswidth = $(TEST_MBWC_ENV)
WRAPPER_tst_wcsxfrm = $(TEST_MBWC_ENV)
WRAPPER_tst_wctob = $(TEST_MBWC_ENV)
WRAPPER_tst_wctomb = $(TEST_MBWC_ENV)
WRAPPER_tst_wctrans = $(TEST_MBWC_ENV)
WRAPPER_tst_wctype = $(TEST_MBWC_ENV)
WRAPPER_tst_wcwidth = $(TEST_MBWC_ENV)

WRAPPER_tst-digits = $(TEST_MBWC_ENV)
WRAPPER_tst-mbswcs6 = $(TEST_MBWC_ENV)
WRAPPER_tst-xlocale1 = $(TEST_MBWC_ENV)
WRAPPER_tst-xlocale2 = $(TEST_MBWC_ENV)
WRAPPER_tst-strfmon1 = $(TEST_MBWC_ENV)
WRAPPER_tst-strptime = $(TEST_MBWC_ENV)
WRAPPER_tst-setlocale = $(TEST_MBWC_ENV) LC_ALL=ja_JP.EUC-JP
WRAPPER_bug-iconv-trans = $(TEST_MBWC_ENV)
WRAPPER_tst-sscanf = $(TEST_MBWC_ENV)
WRAPPER_tst-leaks = MALLOC_TRACE=tst-leaks.mtrace $(TEST_MBWC_ENV)

mtrace-tst-leaks: tst-leaks.out
	../malloc/mtrace tst-leaks.mtrace > $@

include ../Test.mak

TOBEINSTALLED_OTHERS=../localedata/SUPPORTED ../localedata/CHECKSUMS \
	            ../localedata/*.data \
							../localedata/charmaps ../localedata/locales \
							../localedata/tests ../localedata/tst-wctype.input \
							../localedata/tst-fmon-locales \
	            ../localedata/tst-digits.c ../localedata/tst-setlocale.c \
							../localedata/bug-iconv-trans.c ../localedata/tst-leaks.c \
							../localedata/tst-mbswcs6.c ../localedata/tst-xlocale1.c \
							../localedata/tst-xlocale2.c ../localedata/bug-usesetlocale.c \
							../localedata/tst-strfmon1.c ../localedata/tst-sscanf.c \
							../localedata/tst-strptime.c

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing localedata....."
	@cp -fr $(OBJPFX)/../localedata/*.in .
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
