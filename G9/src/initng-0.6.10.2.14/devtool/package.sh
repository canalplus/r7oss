#!/bin/sh

# space separated list of directories and files that should be excluded from distribution
EXCLUDE="devtool/*.png"

# First do some sanity checks
if [ ! -f config.h.cmake ] ; then
	echo "This script has to be run from the toplevel source dir"
	exit 1
fi
if [ ! -d .svn ] ; then
	echo "This script is meant to be run from within an SVN checkout"
	exit 1
fi

# Determine the version number
VERSION=`sed -ne '{s/  *//g;s/^SET(VERSION"\([.0-9][.0-9]*\)"CACHESTRING"Versionnumberoftheproject").*$/\1/p}' < CMakeLists.txt`
EXPORT_DIR="initng-$VERSION"

echo "Exporting source tree"
svn export . $EXPORT_DIR

echo "Removing files/dirs from export dir:"
for i in $EXCLUDE ; do
	echo "  $i"
	rm -rf $EXPORT_DIR/$i
done

echo "Creating tarballs"
tar -czf initng-$VERSION.tar.gz $EXPORT_DIR
tar -cjf initng-$VERSION.tar.bz2 $EXPORT_DIR

echo "Cleaning up"
rm -rf $EXPORT_DIR
