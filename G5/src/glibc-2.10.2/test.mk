# Copyright (C) 2009
# STMicroelectronics, Ltd.
# Author: Salvatore Cro <salvatore.cro@st.com>
#
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
# glibc test suite installation
#

.PHONY : $(TOBEINSTALLED_GEN) $(addprefix tests-install-,$(TESTGORUPS))

TESTGROUPS=$(shell cd $(TESTDIR) && grep -r "install:" */Makefile | awk -F "\/" '{print $$1}')
OBJPFX=$(shell pwd)/objdir

TOBEINSTALLED_GEN= $(OBJPFX)/config.make
TOBEINSTALLED_BIN="$(shell cd $(OBJPFX) && find $* -name '*' -type f -perm 755 ! -name '*.so*' | sort)"
TOBEINSTALLED_SH="$(addprefix ../,$(shell cd $(OBJPFX)/.. && find $* -name '*.sh' | sort))"

tests-install : tests-gen-install $(addprefix tests-install-,$(TESTGROUPS))

tests-gen-install: $(TOBEINSTALLED_GEN)
	@cp -fr $(foreach f,$^, $f) $(TESTDIR)

tests-install-%:
	$(MAKE) -C $(TESTDIR)/$* OBJPFX=$(OBJPFX) PFX=$(PFX) TOBEINSTALLED_BIN=$(TOBEINSTALLED_BIN) TOBEINSTALLED_SH=$(TOBEINSTALLED_SH) install

