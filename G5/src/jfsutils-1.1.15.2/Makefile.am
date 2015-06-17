
SUBDIRS = libfs include fsck fscklog logdump mkfs tune xpeek

EXTRA_DIST = jfsutils.spec.in README.wyplay

dist-hook: jfsutils.spec
	cp jfsutils.spec $(distdir)
	echo "#define JFSUTILS_DATE \"18-Jun-2013\"" > $(distdir)/include/jfs_version.h
