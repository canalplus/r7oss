# Copyright (C) 1995-2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.
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

include ../config.make

ifeq (yes,$(build-shared))
TESTS = glrefmain failtest tst-dladdr default errmsg1 tstcxaatexit \
	bug-dlopen1 bug-dlsym1 tst-dlinfo bug-atexit1 bug-atexit2 \
	bug-atexit3 tstatexit
endif

# following test needs cpp so doesn't
# get compiled, we skip it here

TESTS_DISABLED=bug-atexit3

modules-names = glreflib1 glreflib2 glreflib3 failtestmod defaultmod1 \
		defaultmod2 errmsg1mod modatexit modcxaatexit \
		bug-dlsym1-lib1 bug-dlsym1-lib2 bug-atexit1-lib \
		bug-atexit2-lib bug-atexit3-lib

WRAPPER_glrefmain = LD_LIBRARY_PATH=./
WRAPPER_failtest = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_tst-dladdr = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_errmsg1 = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_tstcxaatexit = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_tst-dlinfo = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_tstatexit = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_default = LD_LIBRARY_PATH=./
WRAPPER_bug-dlsym1 = LD_LIBRARY_PATH=./

failtestmod.so-no-z-defs = yes
glreflib2.so-no-z-defs = yes
errmsg1mod.so-no-z-defs = yes

ifeq (yesyesyes,$(build-static)$(build-shared)$(elf))
TESTS += tststatic tststatic2
modules-names += modstatic modstatic2
WRAPPER_tststatic = LD_LIBRARY_PATH=./:../:../elf
WRAPPER_tststatic2 = LD_LIBRARY_PATH=./:../:../elf
endif

include ../Test.mak

test-modules = $(addsuffix .so,$(modules-names))

ifeq ($(build-shared),yes)
# Build all the modules even when not actually running test programs.
tests: $(test-modules)
endif

TOBEINSTALLED_OTHERS= dlfcn/*.so

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing dlfcn....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
