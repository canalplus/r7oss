# Copyright (C) 1998,1999,2000,2002,2005,2007 Free Software Foundation, Inc.
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
#	Makefile for timezone information
#

others	:= zdump zic
TESTS	:= test-tz tst-timezone

include ../config.make

tzbases := africa antarctica asia australasia europe northamerica \
	   southamerica etcetera factory \
	   solar87 solar88 solar89
tzlinks := backward systemv
tzfiles := $(tzbases) $(tzlinks)
# pacificnew doesn't compile; if it is to be used, it should be included in
# northamerica.
distribute += $(tzfiles) leapseconds pacificnew simplebackw

generated := $(addprefix z.,$(tzfiles))

generated-dirs = testdata

all: # Make this the default target; it will be defined in Rules.

# z.* use this variable.
define nl


endef
ifndef avoid-generated
ifndef inhibit_timezone_rules
-include $(addprefix z.,$(tzfiles))
endif
endif

# Make these absolute file names.
installed-localtime-file := $(firstword $(filter /%,$(inst_localtime-file)) \
					$(addprefix $(inst_zonedir)/, \
						    $(localtime-file)))
installed-posixrules-file := $(firstword $(filter /%,$(posixrules-file)) \
					 $(addprefix $(inst_zonedir)/, \
						     $(posixrules-file)))

# Don't try to install the zoneinfo files since we can't run zic.
install-others = $(addprefix $(inst_zonedir)/,$(zonenames) \
					      $(zonenames:%=posix/%) \
					      $(zonenames:%=right/%)) \
		 $(installed-localtime-file) $(installed-posixrules-file)

ifeq ($(have-ksh),yes)
generated += tzselect
endif

.PHONY: echo-zonenames
echo-zonenames:
	@echo 'Known zones: $(zonenames)'

built-program-file = $(dir $(word 2,$^))$(notdir $(word 2,$^))
ifeq (yesyes,$(build-shared)$(elf))
comma = ,
sysdep-library-path = \
$(subst $(empty) ,:,$(strip $(patsubst -Wl$(comma)-rpath-link=%, %,\
				       $(filter -Wl$(comma)-rpath-link=%,\
						$(sysdep-LDFLAGS)))))
run-program-prefix = $(if $(filter $(notdir $(built-program-file)),\
				   $(tests-static) $(xtests-static)),, \
			  $(elf-objpfx)$(rtld-installed-name) \
			  --library-path $(rpath-link)$(patsubst %,:%,$(sysdep-library-path)))
else
run-program-prefix =
endif
# Never use $(run-program-prefix) for the statically-linked %-bp test programs
built-program-cmd = $(patsubst %,$(run-program-prefix),\
			$(filter-out %-bp,$(built-program-file))) \
		    $(built-program-file)



# We have to use `-d $(inst_zonedir)' to explictly tell zic where to
# place the output files although $(zonedir) is compiled in.  But the
# user might have set $(install_root) on the command line of `make install'.
zic-cmd = $(built-program-cmd) -d $(inst_zonedir)

# The source files specify the zone names relative to the -d directory,
# so for the posix/ and right/ flavors we need to pass -d $(inst_zonedir)/posix
# and the like.  This magic extracts /posix or /right if it's the first
# component after $(inst_zonedir) in the target name $@.
target-zone-flavor = $(filter /posix /right, \
			      /$(firstword $(subst /, , \
					  $(patsubst $(inst_zonedir)/%,%,$@))))

ifdef localtime
$(installed-localtime-file): $(inst_zonedir)/$(localtime) $(objpfx)zic \
			     $(+force)
	$(make-target-directory)
	if test -r $@; then \
	  echo Site timezone NOT reset to Factory.; \
	else \
	  rm -f $@T; \
	  $(SHELL) $(..)scripts/rellns-sh $< $@T; \
	  mv -f $@T $@; \
	fi
endif
ifdef posixrules
$(installed-posixrules-file): $(inst_zonedir)/$(posixrules) $(objpfx)zic \
			      $(+force)
	$(zic-cmd) -p $(posixrules)
endif


$(objpfx)zic: $(objpfx)scheck.o $(objpfx)ialloc.o

tz-cflags = -DTZDIR='"$(zonedir)"' \
	    -DTZDEFAULT='"$(localtime-file)"' \
	    -DTZDEFRULES='"$(posixrules-file)"' \
	    -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone

# We have to make sure the data for testing the tz functions is available.
# Don't add leapseconds here since test-tz made checks that work only without
# leapseconds.
testdata = testdata
define build-testdata
GCONV_PATH=${common-objpfx}iconvdata LANGUAGE=C LC_ALL=C \
  $(built-program-cmd) -d $(testdata) -y ./yearistype $<
endef

test-tz.out: $(addprefix $(testdata)/, America/New_York Etc/UTC UTC)
tst-timezone.out: $(addprefix $(testdata)/, \
				       Europe/Berlin Universal \
				       Australia/Melbourne \
				       America/Sao_Paulo Asia/Tokyo \
				       Europe/London)

WRAPPER_test-tz = TZDIR=$(testdata)
WRAPPER_tst-timezone = TZDIR=$(testdata)

# Note this must come second in the deps list for $(built-program-cmd) to work.
zic-deps = $(objpfx)zic $(leapseconds) yearistype

$(testdata)/America/New_York: northamerica $(zic-deps)
	$(build-testdata)
$(testdata)/Etc/UTC: etcetera $(zic-deps)
	$(build-testdata)
# Use a pattern rule to indicate the command produces both targets at once.
# Two separate targets built separately can collide if in parallel.
%/UTC %/Universal: simplebackw $(zic-deps) %/Etc/UTC
	$(build-testdata)
$(testdata)/%/Berlin $(testdata)/%/London: europe $(zic-deps)
	$(build-testdata)
$(testdata)/Australia/Melbourne: australasia $(zic-deps)
	$(build-testdata)
$(testdata)/America/Sao_Paulo: southamerica $(zic-deps)
	$(build-testdata)
$(testdata)/Asia/Tokyo: asia $(zic-deps)
	$(build-testdata)


$(addprefix $(inst_zonedir)/,iso3166.tab zone.tab): \
    $(inst_zonedir)/%: % $(+force)
	$(do-install)

include ../Test.mak

TOBEINSTALLED_OTHERS= timezone/z.*

TZDATA_BIN=timezone/zic timezone/zdump
LAST_TOBEINSTALLED_BIN=$(filter-out $(TZDATA_BIN),$(TOBEINSTALLED_BIN))

TOBEINSTALLED=$(LAST_TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing timezone....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
