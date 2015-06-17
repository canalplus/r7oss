# Copyright (C) 1991-1999, 2000-2006, 2007, 2009 Free Software Foundation, Inc.
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
#	Sub-makefile for POSIX portion of the library.
#

include ../config.make
rtld-installed-name=ld.so

TESTS		:= tstgetopt testfnm runtests runptests	     \
		   tst-preadwrite tst-preadwrite64 test-vfork regexbug1 \
		   tst-getlogin tst-mmap tst-getaddrinfo tst-truncate \
		   tst-truncate64 tst-fork tst-fnmatch tst-regexloc tst-dir \
		   tst-chmod bug-regex1 bug-regex2 bug-regex3 bug-regex4 \
		   tst-gnuglob tst-regex bug-regex5 bug-regex6 bug-regex7 \
		   bug-regex8 bug-regex9 bug-regex10 bug-regex11 bug-regex12 \
		   bug-regex13 bug-regex14 bug-regex15 bug-regex16 \
		   bug-regex17 bug-regex18 bug-regex19 bug-regex20 \
		   bug-regex21 bug-regex22 bug-regex23 bug-regex24 \
		   bug-regex25 bug-regex26 bug-regex27 bug-regex28 \
		   tst-nice tst-nanosleep tst-regex2 \
		   transbug tst-rxspencer tst-pcre tst-boost \
		   bug-ga1 tst-vfork1 tst-vfork2 tst-vfork3 tst-waitid \
		   tst-getaddrinfo2 bug-glob1 bug-glob2 tst-sysconf \
		   tst-execvp1 tst-execvp2 tst-execlp1 tst-execlp2 \
		   tst-execv1 tst-execv2 tst-execl1 tst-execl2 \
		   tst-execve1 tst-execve2 tst-execle1 tst-execle2 \
		   tst-execvp3 tst-execvp4 tst-rfc3484 tst-rfc3484-2 \
		   tst-rfc3484-3 \
		   tst-getaddrinfo3 tst-fnmatch2 tst-cpucount tst-cpuset
xtests		:= bug-ga2

# Require mtrace on the target
TESTS_DISABLED = bug-regex2 bug-regex14 bug-regex21 tst-vfork3 tst-rxspencer tst-pcre tst-boost bug-ga2 bug-glob2 tst-fnmatch

ifeq (yes,$(build-shared))
TESTS           += wordexp-test tst-exec tst-spawn
endif

ifeq (yes,$(build-static-nss))
# We need it for "make check" only. We can skip them if they haven't
# been built yet during "make".
otherlibs += $(wildcard $(nssobjdir)/libnss_files.a \
			$(resolvobjdir)/libnss_dns.a \
			$(resolvobjdir)/libresolv.a)
endif

# globtest and wordexp-test currently only works with shared libraries
ifeq (yes,$(build-shared))
globtest.out: globtest.sh globtest
	$(SHELL) -e globtest.sh ../ ../elf/ \
		 $(rtld-installed-name)
wordexp-tst.out: wordexp-tst.sh wordexp-test
	$(SHELL) -e wordexp-tst.sh ../ ../elf/ \
		 $(rtld-installed-name)
endif

OPTS_tstgetopt = -a -b -cfoobar --required foobar --optional=bazbug \
		--none random --col --color --colour

OPTS_tst-exec = -- $(built-program-cmd)
OPTS_tst-spawn = -- $(built-program-cmd)
OPTS_tst-dir = `pwd` `cd ../posix; pwd` `cd ..; pwd` tst-dir
OPTS_tst-chmod = `pwd`
OPTS_tst-vfork3 = --test-dir=.

WRAPPER_tst-fnmatch = LOCPATH=../localedata
WRAPPER_tst-regexloc = LOCPATH=../localedata
WRAPPER_bug-regex1 = LOCPATH=../localedata
WRAPPER_tst-regex = LOCPATH=../localedata
WRAPPER_tst-regex2 = LOCPATH=../localedata
WRAPPER_bug-regex5 = LOCPATH=../localedata
WRAPPER_bug-regex6 = LOCPATH=../localedata
WRAPPER_bug-regex17 = LOCPATH=../localedata
WRAPPER_bug-regex18 = LOCPATH=../localedata
WRAPPER_bug-regex19 = LOCPATH=../localedata
WRAPPER_bug-regex20 = LOCPATH=../localedata
WRAPPER_bug-regex22 = LOCPATH=../localedata
WRAPPER_bug-regex23 = LOCPATH=../localedata
WRAPPER_bug-regex25 = LOCPATH=../localedata
WRAPPER_bug-regex26 = LOCPATH=../localedata
OPTS_tst-rxspencer = --utf8 rxspencer/tests
WRAPPER_tst-rxspencer = LOCPATH=../localedata
OPTS_tst-pcre = PCRE.tests
OPTS_tst-boost = BOOST.tests
OPTS_bug-glob1 = "."
OPTS_tst-execvp3 = --test-dir=.

include ../Test.mak

testcases.h: TESTS TESTS2C.sed
	sed -f TESTS2C.sed < $< > $@T
	mv -f $@T $@
ifeq ($(with-cvs),yes)
	test ! -d CVS || cvs $(CVSOPTS) commit -mRegenerated $@
endif

ptestcases.h: PTESTS PTESTS2C.sed
	sed -f PTESTS2C.sed < $< > $@T
	mv -f $@T $@
ifeq ($(with-cvs),yes)
	test ! -d CVS || cvs $(CVSOPTS) commit -mRegenerated $@
endif

annexc.out: annexc
	-$(dir $<)$(notdir $<) '$(CC)' \
	  '$(patsubst %,-I../%,$(sorted-subdirs)) -I../include $(+sysdep-includes) $(sysincludes) -I..' > $@

WRAPPER_bug-regex2 = MALLOC_TRACE=bug-regex2.mtrace
WRAPPER_bug-regex14 = MALLOC_TRACE=bug-regex14.mtrace
WRAPPER_bug-regex21 = MALLOC_TRACE=bug-regex21.mtrace
WRAPPER_tst-vfork3 = MALLOC_TRACE=tst-vfork3.mtrace
WRAPPER_tst-pcre = MALLOC_TRACE=tst-pcre.mtrace
WRAPPER_tst-boost = MALLOC_TRACE=tst-boost.mtrace
WRAPPER_bug-ga2 = MALLOC_TRACE=bug-ga2.mtrace
WRAPPER_bug-glob2 = MALLOC_TRACE=bug-glob2.mtrace

tst-getconf.out: tst-getconf.sh getconf
	$(SHELL) -e $< ../ ../elf/ $(rtld-installed-name)

TOBEINSTALLED_OTHERS=../posix/*.input \
	../ChangeLog.8 \
	../ChangeLog.14

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing posix...."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
