# Copyright (C) 1995-2007, 2008, 2009 Free Software Foundation, Inc.
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

# Makefile for elf subdirectory of GNU C Library.

EXTRA_SPECFLAGS=

rtld-installed-name=./ld.so

include ../config.make

TESTS = tst-tls1 tst-tls2 tst-tls9 tst-leaks1
ifeq (yes,$(have-initfini-array))
TESTS += tst-array1 tst-array2 tst-array3 tst-array4 tst-array5
endif
ifeq (yes,$(build-static))
tests-static = tst-tls1-static tst-tls2-static tst-stackguard1-static
ifeq (yesyes,$(build-shared)$(elf))
tests-static += tst-tls9-static
WRAPPER_tst-tls9-static = LD_LIBRARY_PATH=./:../:../dlfcn
endif
ifeq (yes,$(have-initfini-array))
tests-static += tst-array1-static tst-array5-static
endif
TESTS += $(tests-static)
endif
ifeq (yes,$(build-shared))
TESTS += loadtest restest1 preloadtest loadfail multiload origtest resolvfail \
	 constload1 order vismain noload filter unload \
	 reldep reldep2 reldep3 reldep4 nodelete nodelete2 \
	 nodlopen nodlopen2 neededtest neededtest2 \
	 neededtest3 neededtest4 unload2 lateglobal initfirst global \
	 restest2 next dblload dblunload reldep5 reldep6 reldep7 reldep8 reldep9 \
	 circleload1 tst-tls3 tst-tls4 tst-tls5 tst-tls6 tst-tls7 tst-tls8 \
	 tst-tls10 tst-tls11 tst-tls12 tst-tls13 tst-tls14 tst-tls15 \
	 tst-tls16 tst-tls17 tst-tls18 tst-tls-dlinfo \
	 tst-align tst-align2 $(tests-execstack-yes) \
	 tst-dlmodcount tst-dlopenrpath tst-deep1 \
	 tst-dlmopen1 tst-dlmopen2 tst-dlmopen3 \
	 unload3 unload4 unload5 unload6 unload7 tst-global1 order2 \
	 tst-audit1 tst-audit2 \
	 tst-stackguard1 tst-addr1 tst-thrlock
ifeq (yes,$(have-fpie))
TESTS +=tst-pie
endif
endif
tests-execstack-yes = tst-execstack tst-execstack-needed tst-execstack-prog
TESTS_DISABLED += $(tests-execstack-yes)

ifeq (yes,$(build-shared))
TESTS += tst-pathopt
endif

ifeq ($(build-shared),yes)
# Build all the modules even when not actually running test programs.
tests: $(test-modules)
endif

loadtest.out: $(test-modules)

WRAPPER_neededtest4 = LC_ALL=C LANGUAGE=C

preloadtest-preloads = testobj1 testobj2 testobj3 testobj4 testobj5
WRAPPER_preloadtest = \
  LD_LIBRARY_PATH=. LD_PRELOAD=$(subst $(empty) ,:,$(strip $(preloadtest-preloads:=.so)))

order.out: order
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@./$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  ./order > $@
	@(echo "0123456789" | cmp $@ -) > /dev/null

WRAPPER_vismain = LD_PRELOAD=vismod3.so

tst-pathopt.out: tst-pathopt.sh ./tst-pathopt \
			  ./pathoptobj.so
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(SHELL) -e $< ../

WRAPPER_unload7 = MALLOC_PERTURB_=85 LD_LIBRARY_PATH=./:

tst-array1.out: tst-array1
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  tst-array1 > $@
	cmp $@ tst-array1.exp > /dev/null

tst-array1-static.out: tst-array1-static
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@tst-array1-static > $@
	cmp $@ tst-array1.exp > /dev/null

tst-array2.out: tst-array2
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  tst-array2 > $@
	cmp $@ tst-array2.exp > /dev/null

tst-array3.out: tst-array3
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  tst-array3 > $@
	cmp $@ tst-array1.exp > /dev/null

tst-array4.out: tst-array4 tst-array2dep.so
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  $< > $@
	cmp $@ tst-array4.exp > /dev/null

tst-array5.out: tst-array5
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  tst-array5 > $@
	cmp $@ tst-array5.exp > /dev/null

tst-array5-static.out: tst-array5-static
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@tst-array5-static > $@
	cmp $@ tst-array5-static.exp > /dev/null

ifeq (yesyes,$(have-fpie)$(build-shared))
tst-pie1.out: tst-pie1
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@./$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  $< > $@
endif

ifeq (yes,$(build-shared))
check-textrel.out: check-textrel
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(dir $<)$(notdir $<) ../libc.so \
	  $(sort $(wildcard ../lib*.so \
			    ../iconvdata/*.so)) > $@
endif

check-data := $(firstword $(wildcard \
	        $(foreach D,$(add-ons) scripts,\
				$(patsubst %,$(..)$D/data/localplt-%.data,\
					$(abi-name) \
					$(addsuffix -$(config-os),\
					$(config-machine) \
					$(base-machine)) \
					generic))))

ifeq ($(have-thread-library),yes)
thread-dso := $(filter-out %_nonshared.a, $(shared-thread-library))
endif

check-localplt.out: check-localplt \
					../libc.so \
					../math/libm.so $(thread-dso) \
					../rt/librt.so \
					../dlfcn/libdl.so \
					../resolv/libresolv.so \
					../crypt/libcrypt.so \
					$(check-data)
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(dir $<)$(notdir $<) $(filter-out $< $(check-data),$^) | \
	  LC_ALL=C sort | \
	  diff -u $(check-data) - > $@

WRAPPER_tst-audit1 = LD_AUDIT=tst-auditmod1.so
WRAPPER_tst-audit2 = LD_AUDIT=tst-auditmod1.so

order2.out: order2 ../elf/order2mod1.so ../elf/order2mod2.so
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@../elf/$(rtld-installed-name) \
	  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)) \
	  ./order2 > $@
	@(echo "12345" | cmp $@ -) > /dev/null

OPTS_tst-stackguard1 = --command "$(built-program-cmd) --child"
OPTS_tst-stackguard1-static = --command "./tst-stackguard1-static --child"

WRAPPER_constload1 = LD_LIBRARY_PATH=./

WRAPPER_reldep = LD_LIBRARY_PATH=./
WRAPPER_reldep2 = LD_LIBRARY_PATH=./
WRAPPER_reldep3 = LD_LIBRARY_PATH=./
WRAPPER_reldep7 = LD_LIBRARY_PATH=./
WRAPPER_resolvfail = LD_LIBRARY_PATH=./
WRAPPER_sln = LD_LIBRARY_PATH=./
WRAPPER_sprof = LD_LIBRARY_PATH=./
WRAPPER_tst-align = LD_LIBRARY_PATH=./
WRAPPER_tst-stackguard1 = LD_LIBRARY_PATH=./
WRAPPER_tst-stackguard1-static = LD_LIBRARY_PATH=./

WRAPPER_tst-tls-dlinfo = LD_LIBRARY_PATH=./
WRAPPER_tst-tls4 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls5 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls6 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls9 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls15 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls16 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls18 = LD_LIBRARY_PATH=./

WRAPPER_unload = LD_LIBRARY_PATH=./
WRAPPER_unload2 = LD_LIBRARY_PATH=./
WRAPPER_unload3 = LD_LIBRARY_PATH=./
WRAPPER_unload4 = LD_LIBRARY_PATH=./
WRAPPER_unload5 = LD_LIBRARY_PATH=./
WRAPPER_unload6 = LD_LIBRARY_PATH=./

include ../Test.mak

TOBEINSTALLED_OTHERS=elf/trusted-dirs.* elf/*.so \
	            ../elf/check-textrel.c \
							../elf/check-localplt.c config.h

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
