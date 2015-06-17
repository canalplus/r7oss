# Common makefile rules for tests

# Copyright (C) 2009
# STMicroelectronics, Ltd.
# Author: Salvatore Cro <salvatore.cro@st.com>

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

ifneq ($(TESTS),)
ifneq ($(TESTS_DISABLED),)
TESTS := $(filter-out $(TESTS_DISABLED),$(TESTS))
endif
TARGETS   := $(addsuffix .out,$(TESTS))
CLEAN_TARGETS := $(TARGETS)
endif

built-program-file = $*
rtld-installed-name=ld.so
rpath-link=/lib
ifeq (yesyes,$(build-shared)$(elf))
comma = ,
sysdep-library-path = \
$(subst $(empty) ,:,$(strip $(patsubst -Wl$(comma)-rpath-link=%, %,\
				       $(filter -Wl$(comma)-rpath-link=%,\
						$(sysdep-LDFLAGS)))))
run-program-prefix = $(if $(filter $(notdir $(built-program-file)),\
				   $(tests-static) $(xtests-static)),, \
			  "../elf/"$(rtld-installed-name) \
			  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)))
else
run-program-prefix =
endif

# Never use $(run-program-prefix) for the statically-linked %-bp test programs
built-program-cmd = $(patsubst %,$(run-program-prefix),$(filter-out %-bp,$(built-program-file))) ./$(built-program-file)

define exec_test
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@\
	$(WRAPPER) $(WRAPPER_$(patsubst %_glibc,%,$*)) \
	./$* $(OPTS) $(OPTS_$(patsubst %_glibc,%,$*)) > "$@" 2>&1 ; \
		ret=$$? ; \
		expected_ret="$(RET_$(patsubst %_glibc,%,$*))" ; \
		test -z "$$expected_ret" && export expected_ret=0 ; \
	if ! test $$ret -eq $$expected_ret ; then \
		echo "ret == $$ret ; expected_ret == $$expected_ret" ; \
		exit 1 ; \
	fi
	-@true "$@"
endef

run: $(TARGETS)

%.out:
	$(exec_test)

clean:
	@$(RM) *.o *~ core *.out *.gdb $(EXTRA_CLEAN)
	@$(RM_R) $(EXTRA_DIRS)

.PHONY: all clean run
