## $Id: Makefile.am,v 1.12 2007/11/08 20:58:56 lirc Exp $

## Process this file with automake to produce Makefile.in 

EXTRA_DIST=lirc.css irxevent.keys lirc.hwdb

SUBDIRS = man

lirc.hwdb:
	touch $@

noinst_PROGRAMS = man2html

release-man: man2html
	top_srcdir="$(top_srcdir)" srcdir="$(srcdir)" builddir="$(builddir)" \
			   top_builddir="$(top_builddir)" $(srcdir)/release-man.sh

release-html:
	top_srcdir="$(top_srcdir)" srcdir="$(srcdir)" builddir="$(builddir)" \
			   top_builddir="$(top_builddir)" $(srcdir)/release-html.sh

release: release-man release-html

dist-hook: release
	cp -a html $(srcdir)/images $(distdir)

maintainer-clean-local:
	rm -rf html man-html
