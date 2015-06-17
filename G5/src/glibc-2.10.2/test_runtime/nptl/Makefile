# Copyright (C) 2002,2003,2004,2005,2006,2007,2008
#	Free Software Foundation, Inc.
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
#	Sub-makefile for NPTL portion of the library.
#

include ../config.make

rtld-installed-name=ld.so

TESTS = tst-typesizes \
	tst-attr1 tst-attr2 tst-attr3 \
	tst-mutex1 tst-mutex2 tst-mutex3 tst-mutex4 tst-mutex5 tst-mutex6 \
	tst-mutex7 tst-mutex8 tst-mutex9 tst-mutex5a tst-mutex7a \
	tst-mutexpi1 tst-mutexpi2 tst-mutexpi3 tst-mutexpi4 tst-mutexpi5 \
	tst-mutexpi5a tst-mutexpi6 tst-mutexpi7 tst-mutexpi7a tst-mutexpi8 \
	tst-mutexpi9 \
	tst-spin1 tst-spin2 tst-spin3 \
	tst-cond1 tst-cond2 tst-cond3 tst-cond4 tst-cond5 tst-cond6 tst-cond7 \
	tst-cond8 tst-cond9 tst-cond10 tst-cond11 tst-cond12 tst-cond13 \
	tst-cond14 tst-cond15 tst-cond16 tst-cond17 tst-cond18 tst-cond19 \
	tst-cond20 tst-cond21 tst-cond22 tst-cond23 \
	tst-robust1 tst-robust2 tst-robust3 tst-robust4 tst-robust5 \
	tst-robust6 tst-robust7 tst-robust8 tst-robust9 \
	tst-robustpi1 tst-robustpi2 tst-robustpi3 tst-robustpi4 tst-robustpi5 \
	tst-robustpi6 tst-robustpi7 tst-robustpi8 tst-robustpi9 \
	tst-rwlock1 tst-rwlock2 tst-rwlock2a tst-rwlock3 tst-rwlock4 \
	tst-rwlock5 tst-rwlock6 tst-rwlock7 tst-rwlock8 tst-rwlock9 \
	tst-rwlock10 tst-rwlock11 tst-rwlock12 tst-rwlock13 tst-rwlock14 \
	tst-once1 tst-once2 tst-once3 tst-once4 \
	tst-key1 tst-key2 tst-key3 tst-key4 \
	tst-sem1 tst-sem2 tst-sem3 tst-sem4 tst-sem5 tst-sem6 tst-sem7 \
	tst-sem8 tst-sem9 tst-sem10 tst-sem11 tst-sem12 \
	tst-barrier1 tst-barrier2 tst-barrier3 tst-barrier4 \
	tst-align tst-align2 tst-align3 \
	tst-basic1 tst-basic2 tst-basic3 tst-basic4 tst-basic5 tst-basic6 \
	tst-basic7 \
	tst-kill1 tst-kill2 tst-kill3 tst-kill4 tst-kill5 tst-kill6 \
	tst-raise1 \
	tst-join1 tst-join2 tst-join3 tst-join4 tst-join5 tst-join6 \
	tst-detach1 \
	tst-eintr1 tst-eintr2 tst-eintr3 tst-eintr4 tst-eintr5 \
	tst-tsd1 tst-tsd2 tst-tsd3 tst-tsd4 tst-tsd5 tst-tsd6 \
	tst-tls1 tst-tls2 \
	tst-fork1 tst-fork2 tst-fork3 tst-fork4 \
	tst-atfork1 \
	tst-cancel1 tst-cancel2 tst-cancel3 tst-cancel4 tst-cancel5 \
	tst-cancel6 \
	tst-cancel7 tst-cancel8 tst-cancel9 tst-cancel10 \
	tst-cancel11 tst-cancel12 tst-cancel13 tst-cancel14 tst-cancel15 \
	tst-cancel16 tst-cancel17 tst-cancel18 tst-cancel19 tst-cancel20 \
	tst-cancel21 tst-cancel22 tst-cancel23 tst-cancel24 tst-cancel25 \
	tst-cleanup0 tst-cleanup1 tst-cleanup2 tst-cleanup3 tst-cleanup4 \
	tst-flock1 tst-flock2 \
	tst-signal1 tst-signal2 tst-signal3 tst-signal4 tst-signal5 \
	tst-signal6 tst-signal7 \
	tst-exec1 tst-exec2 tst-exec3 tst-exec4 \
	tst-exit1 tst-exit2 tst-exit3 \
	tst-stdio1 tst-stdio2 \
	tst-stack1 tst-stack2 tst-stack3 \
	tst-unload \
	tst-dlsym1 \
	tst-sysconf \
	tst-locale1 tst-locale2 \
	tst-umask1 \
	tst-popen1 \
	tst-clock1 \
	tst-context1 \
	tst-sched1 \
	tst-backtrace1 \
	tst-oddstacklimit \
	tst-vfork1 tst-vfork2 tst-vfork1x tst-vfork2x \
	tst-getpid1 tst-getpid2 tst-getpid3 \
	tst-initializers1 $(patsubst %,tst-initializers1-%,c89 gnu89 c99 gnu99)
xtests = tst-setuid1 tst-setuid1-static tst-mutexpp1 tst-mutexpp6 tst-mutexpp10

# This sets the stack resource limit to 1023kb, which is not a multiple
# of the page size since every architecture's page size is > 1k.
WRAPPER_tst-oddstacklimit = ulimit -s 1023; LD_LIBRARY_PATH=./

ifeq ($(have-forced-unwind),yes)
TESTS += tst-cancelx2 tst-cancelx3 tst-cancelx4 tst-cancelx5 \
	 tst-cancelx6 tst-cancelx7 tst-cancelx8 tst-cancelx9 tst-cancelx10 \
	 tst-cancelx11 tst-cancelx12 tst-cancelx13 tst-cancelx14 tst-cancelx15 \
	 tst-cancelx16 tst-cancelx17 tst-cancelx18 tst-cancelx20 tst-cancelx21 \
	 tst-cleanupx0 tst-cleanupx1 tst-cleanupx2 tst-cleanupx3 tst-cleanupx4 \
	 tst-oncex3 tst-oncex4
endif
ifeq ($(build-shared),yes)
TESTS += tst-atfork2 tst-tls3 tst-tls4 tst-tls5 tst-_res1 tst-fini1 \
	 tst-stackguard1
tests-nolibpthread += tst-fini1
ifeq ($(have-z-execstack),yes)
TESTS += tst-execstack
endif
endif

# Require mtrace on the target
TESTS_DISABLED += tst-stack3 tst-atfork2

ifeq ($(build-static),yes)
tests-static += tst-locale1 tst-locale2 tst-stackguard1-static
TESTS += tst-stackguard1-static
xtests-static += tst-setuid1-static
endif
# These tests are linked with libc before libpthread
tests-reverse += tst-cancel5 tst-cancel23 tst-vfork1x tst-vfork2x


OPTS_tst-cancel7 = --command "./tst-cancel7"
OPTS_tst-cancelx7 = $(OPTS_tst-cancel7)
OPTS_tst-umask1 = tst-umask1.temp

WRAPPER_tst-atfork2 = MALLOC_TRACE=tst-atfork2.mtrace

WRAPPER_tst-stack4 = LD_LIBRARY_PATH=./

ifeq ($(build-shared),yes)
tst-tls6.out: tst-tls6.sh tst-tls5 \
		       tst-tls5moda.so tst-tls5modb.so \
		       tst-tls5modc.so tst-tls5modd.so \
		       tst-tls5mode.so tst-tls5modf.so
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(SHELL) -e tst-tls6.sh ../ ../elf/ \
		    $(rtld-installed-name)
endif

ifeq ($(build-shared),yes)
vpath pt-initfini.c $(sysdirs)

make-test-out = GCONV_PATH=../iconvdata LC_ALL=C \
		$(WRAPPER_$*) ./$* $(OPTS_$*)
tst-cleanup0.out: /dev/null tst-cleanup0
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(make-test-out) 2>&1 | cmp - tst-cleanup0.expect >& $@

# We only have one kind of startup code files.  Static binaries and
# shared libraries are build using the PIC version.

endif

ifeq (yes,$(build-shared))
tst-cancel-wrappers.out: tst-cancel-wrappers.sh
	@echo "  TEST_EXEC $(notdir $(CURDIR))/ $*"
	@$(SHELL) $< ./libc_pic.a \
		    /usr/lib/libc.a \
		    ./libpthread_pic.a \
		    /usr/lib/libpthread.a > $@
endif

OPTS_tst-exec4 = $(built-program-cmd)

OPTS_tst-stackguard1 = --command "$(built-program-cmd) --child"
OPTS_tst-stackguard1-static = --command "./tst-stackguard1-static --child"

WRAPPER_tst-tls3 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls4 = LD_LIBRARY_PATH=./
WRAPPER_tst-tls5 = LD_LIBRARY_PATH=./
WRAPPER_tst-_res1 = LD_LIBRARY_PATH=./
WRAPPER_tst-atfork2 = LD_LIBRARY_PATH=./
WRAPPER_tst-execstack = LD_LIBRARY_PATH=./
WRAPPER_tst-fini1 = LD_LIBRARY_PATH=./

# generated on the fly during compilation and added here
# (is it valuable and/or correct to do so ??) :
TESTS +=test-as-const-lowlevelbarrier test-as-const-lowlevelcond test-as-const-lowlevelrobustlock test-as-const-lowlevelrwlock test-as-const-pthread-errnos test-as-const-pthread-pi-defines test-as-const-structsem test-as-const-unwindbuf

# The tests here better do not run in parallel
ifneq ($(filter %tests,$(MAKECMDGOALS)),)
.NOTPARALLEL:
endif

include ../Test.mak

TOBEINSTALLED_OTHERS=nptl/*.so \
					../nptl/*.expect \
					libc_pic.a \
					nptl/libpthread_pic.a

TOBEINSTALLED=$(TOBEINSTALLED_BIN) $(TOBEINSTALLED_SH) $(TOBEINSTALLED_OTHERS)

.PHONY: $(TOBEINSTALLED)

install: $(TOBEINSTALLED)
	@echo "Installing nptl....."
	@cp -fr $(foreach f,$^, $(OBJPFX)/$f) .
