## $Id: Makefile.am,v 5.12 2009/07/07 20:44:44 lirc Exp $

## Process this file with automake to produce Makefile.in 
## Makefile.am, (c)1999 Tom Wheeley <tw104@york.ac.uk>

## we need version 1.5 of automake for DIST_SUBDIRS to work and dist-bzip2.
AUTOMAKE_OPTIONS = 1.5 check-news dist-bzip2

EXTRA_DIST =  setup.sh setup-driver.sh setup.data data2setup.sh ANNOUNCE TODO

## daemons/ and tools/ should appear before doc/ for help2man to work.
SUBDIRS = daemons tools doc

## From automake documentation:
## Note that EXTRA_DIST can only handle files in the current
## directory; files in other directories will cause make dist runtime
## failures.

## ----------------------------------------------------------------------
## flags to pass to ./configure when calling "make distcheck" and "make
## distcheck-light". Try to check as many features as possible! Also
## need to give enable-sandboxed, so that staged install does not fail.

## !!! BIG WARNING: this needs to be tweaked a bit more !!! ##

DISTCHECK_FLAGS = --with-driver=all --enable-sandboxed
DISTCHECK_CONFIGURE_FLAGS = ${DISTCHECK_FLAGS}
DISTCHECK_LIGHT_FLAGS = --with-driver=none --enable-sandboxed

distcheck-light:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_LIGHT_FLAGS)" distcheck

# ----------------------------------------------------------------------

setup-driver.sh: data2setup.sh setup.data
	./data2setup.sh > setup-driver.sh

dist-hook:
	(cd $(srcdir)/contrib/hal/ && ./gen-hal-fdi.pl)
	for f in contrib remotes; do \
		cp -pr $(srcdir)/$$f $(distdir)/$$f; done
	rm -r `find $(distdir) -name "CVS"`
	-rcs2log >$(distdir)/ChangeLog

DISTCLEANFILES = configure.sh .setup.config

CLEANFILES = *~ remotes/*~ contrib/*~
