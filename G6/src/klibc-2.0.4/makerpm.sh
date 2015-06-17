#!/bin/bash -xe
#
# Make an rpm from the current git repository
#

[ -z "$tmpdir" ] && export tmpdir=/var/tmp

./maketar.sh
rpmbuild -ta $tmpdir/klibc-`cat usr/klibc/version`.tar.gz
