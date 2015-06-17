# Copyright (C) 1997-2004,2005,2006,2007,2008 Free Software Foundation, Inc.
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
#	Makefile for iconv data and code.
#

include ../config.make

ifeq (yes,$(build-shared))
TESTS = bug-iconv1 bug-iconv2 tst-loading tst-e2big tst-iconv4 bug-iconv4 \
	tst-iconv6 bug-iconv5 bug-iconv6 tst-iconv7
ifeq ($(have-thread-library),yes)
TESTS += bug-iconv3
endif

WRAPPER_bug-iconv6 = LOCPATH=../localedata
WRAPPER_tst-iconv7 = LOCPATH=../localedata
endif


generated = $(generated-modules:=.h) $(generated-modules:=.stmp) \
	    iconv-test.out iconv-rules tst-loading.mtrace	 \
	    mtrace-tst-loading tst-tables.out iconv-test.xxx

# Rules to generate the headers.
export AWK

define generate-8bit-table
$(make-target-directory)
LC_ALL=C $(SHELL) ./gen-8bit.sh $< > $(@:stmp=T)
$(move-if-change) $(@:stmp=T) $(@:stmp=h)
touch $@
endef

define generate-8bit-gap-table
$(make-target-directory)
LC_ALL=C $(SHELL) ./gen-8bit-gap.sh $< > $(@:stmp=T)
$(move-if-change) $(@:stmp=T) $(@:stmp=h)
touch $@
endef

ifndef avoid-generated
$(objpfx)iconv-rules: Makefile
	$(make-target-directory)
	{ echo $(filter-out lib%, $(modules)); \
	  echo 8bit $(gen-8bit-modules); \
	  echo 8bit-gap $(gen-8bit-gap-modules); } | \
	LC_ALL=C \
	$(AWK) 'NR == 1 { \
		  for (i = 1; i <= NF; i++) { \
		    printf "%s-routines := %s\n", $$i, tolower($$i); \
		    printf "%s-map := gconv.map\n", $$i; \
		  } \
		}; \
		NR > 1 { \
		  for (i = 2; i <= NF; i++) { \
		    printf "$$(objpfx)%s.stmp: ", $$i; \
		    map = toupper($$i); \
		    sub(/^ISO8859/, "ISO-8859", map); \
		    if (map == "ISO_2033") map = map "-1983"; \
		    printf "$$(charmaps)/%s ", map; \
		    printf "gen-%s.sh\n", $$1; \
		    printf "\t$$(generate-%s-table)\n", $$1; \
		  } \
		}' > $@.new
	mv -f $@.new $@
endif

$(generated-modules:%=$(objpfx)%.h): $(objpfx)%.h: $(objpfx)%.stmp
	@:

$(gen-8bit-modules:%=$(objpfx)%.stmp): $(objpfx)%.stmp: $(objpfx)iconv-rules
$(gen-8bit-gap-modules:%=$(objpfx)%.stmp): $(objpfx)%.stmp: $(objpfx)iconv-rules

$(objpfx)iso8859-7jp.stmp: $(charmaps)/ISO-8859-7 gen-8bit-gap-1.sh
	$(make-target-directory)
	LC_ALL=C $(SHELL) ./gen-8bit-gap-1.sh $< > $(@:stmp=T)
	$(move-if-change) $(@:stmp=T) $(@:stmp=h)
	touch $@

headers: $(addprefix $(objpfx), $(generated-modules:=.h))

$(inst_gconvdir)/gconv-modules: gconv-modules $(+force)
	$(do-install)

# Update the $(prefix)/lib/gconv/gconv-modules.cache file. This is necessary
# if this libc has more gconv modules than the previously installed one.
	if test -f "$(inst_gconvdir)/gconv-modules.cache"; then \
	   LC_ALL=C LANGUAGE=C \
	   ../elf/ld.so --library-path $(rpath-link) \
	   ../iconv/iconvconfig \
	     $(addprefix --prefix=,$(install_root)); \
	fi


ifeq (yes,$(build-shared))

PREBUILT_TESTS+= iconv-test tst-tables
prereq_iconv-test= $(modules.so) \
		 ../iconv/iconv_prog TESTS
prereq_tst-tables= $(modules.so) \
		 tst-table-from tst-table-to

#tests: conv-test.out tst-tables.out
ifneq (no,$(PERL))
#PREBUILT_TESTS+= mtrace-tst-loading
endif
endif

WRAPPER_tst-loading = MALLOC_TRACE=tst-loading.mtrace
mtrace-tst-loading: tst-loading.out
	../malloc/mtrace tst-loading.mtrace > $@

bug-iconv1.out: gconv-modules $(modules.so)
bug-iconv2.out: gconv-modules $(modules.so)
bug-iconv3: $(libdl)
bug-iconv3.out: gconv-modules $(modules.so)
bug-iconv5.out: gconv-modules $(modules.so)
tst-loading.out: gconv-modules $(modules.so)
tst-iconv4.out: gconv-modules $(modules.so)
tst-iconv7.out: gconv-modules $(modules.so)

tests: $(foreach t,$PREBUILT_TESTS,$(prereq_$t)) \
	renew_prebuilt_tests \
	$(addsuffix .out, $(PREBUILT_TESTS))

renew_prebuilt_tests:
	@touch $(PREBUILT_TESTS)

iconv-test.out: run-iconv-test.sh gconv-modules
	$(SHELL) -e $< ../ > $@

tst-tables.out: tst-tables.sh gconv-modules
	$(SHELL) $< ../ ../iconvdata/ > $@

do-tests-clean common-mostlyclean: tst-tables-clean

.PHONY: tst-tables-clean
tst-tables-clean:
	-rm -f tst-*.table tst-EUC-TW.irreversible

include ../Test.mak

TOBEINSTALLED_OTHERS= iconvdata/*.so ../iconvdata/gconv-modules \
	             ../iconvdata/TESTS* ../iconvdata/testdata

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing iconvdata....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .

