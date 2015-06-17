#!/bin/sh

###########################################################################
#                                                                         #
# build glibc script - basically builds/installs/packages                 #
#                      libc/tests/sources for sh4, armv5 and armv7        #
#                      architectures                                      #
#                                                                         #
# Copyright (c) 2010, 2011, 2012 STMicroelectronics Ltd                   #
#                                                                         #
# This program is free software;                                          #
# GNU General Public License as                                           #
# published by the Free Software Foundation.                              #
#                                                                         #
###########################################################################

confLib() {

OPTFLAG=$*
#set -x

CONFIG_SHELL="/bin/sh" ; export CONFIG_SHELL ;
CC=$ARCH"-linux-gcc" ; export CC ;
AS=$ARCH"-linux-as" ; export AS ;
LD=$ARCH"-linux-ld" ; export LD ;
OBJDUMP=$ARCH"-linux-objdump" ; export OBJDUMP
AR=$ARCH"-linux-ar" ; export AR ;
RANLIB=$ARCH"-linux-ranlib" ; export RANLIB ;
CXX=$ARCH"-linux-g++" ; export CXX ;
NM=$ARCH"-linux-nm" ; export NM ;
STRIP="true" ; export STRIP ;
SED="sed" ; export SED ;
CFLAGS="-fstack-protector-all -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -Os "$OPTFLAG ; export CFLAGS
CXXFLAGS="-fstack-protector-all -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -Os "$OPTFLAG ; export CXXFLAGS
LIBTOOL_PREFIX_BASE=$DEVKITROOT/target ; export LIBTOOL_PREFIX_BASE
PKG_CONFIG_SYSROOT_DIR=$DEVKITROOT/target ; export PKG_CONFIG_SYSROOT_DIR
PKG_CONFIG_LIBDIR=$DEVKITROOT/target/usr/lib/pkgconfig ; export PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
FREETYPE_CONFIG=$DEVKITROOT/target/bin/$ARCH-linux-freetype-config ; export FREETYPE_CONFIG
FT2_CONFIG=$DEVKITROOT/target/bin/$ARCH-linux-freetype-config ; export FT2_CONFIG
SDL_CONFIG=$DEVKITROOT/target/bin/$ARCH-linux-sdl-config ; export SDL_CONFIG
ACLOCAL_PATH=$DEVKITROOT/target/usr/share/aclocal ; export ACLOCAL_PATH
LDFLAGS=""
CONFIG_SITE=$DEVKITROOT/../../config/config.site.$ARCH ; export CONFIG_SITE
ASFLAGS=' -g' ; export ASFLAGS

BUILD_CC=gcc
export BUILD_CC
unset LD_LIBRARY_PATH

if [ "$ARCH" = "sh4" ] ; then
  mkdir -p objdir
  echo "no-z-defs=yes" > objdir/configparms
  # enable stack smashing protection
  export libc_cv_ssp=yes
fi
export libc_cv_forced_unwind=yes
export libc_cv_c_cleanup=yes

#  we have to unset fortify source for glibc build
export CFLAGS=`echo $CFLAGS | sed -e 's,-D_FORTIFY_SOURCE=2,,'`
#  we have to unset stack smashing protection for glibc build
#  as it is enabled by the autoconf system
export CFLAGS=`echo $CFLAGS | sed -e 's,-fstack-protector-all,,'`
export CFLAGS=`echo $CFLAGS | sed -e 's,-fstack-protector,,'`

  echo "Replacing autofiles and libtool" ;

  if [ -d $AUTOMAKE ]; then
    atbase=$AUTOMAKE ;
    subfiles="`find .  -name config.sub`" ;
    guessfiles="`find . -name config.guess`" ;
    if [ -n "$subfiles" ]; then
      for sbf in $subfiles; do
        rm -f $sbf ;
        cp $atbase/config.sub $sbf ;
      done
    fi
    if [ -n "$guessfiles" ]; then
      for cnf in $guessfiles; do
        rm -f $cnf ;
        cp $atbase/config.guess $cnf ;
      done
    fi ;
  fi;

  for ltmain in `find . -maxdepth 4 -name ltmain.sh -print` ; do
    dn=`dirname $ltmain`;
    cn=`basename $ltmain`;
    echo "cd $dn";
    pushd $dn > /dev/null ;
    if [  ! '(' -f configure.ac -o -f configure.in ')' ]; then
      popd > /dev/null ;
    if [  ! '(' -f configure.ac -o -f configure.in ')' ]; then
		echo "************* Creating dummy configure.ac ************" ;
		touch configure.ac;
	fi;
        libtoolize --force --copy --install;
    else
        libtoolize --force --copy --install;
        popd > /dev/null ;
    fi;
  done ;
  echo "Done replacing libtool" ;

if [ "$ARCH" = "sh4" ] ; then
  ARCHSPEC_ENABLES="	--host=sh4-linux \
	--enable-add-ons=nptl \
	--enable-stackguard-randomization"
else
  ARCHSPEC_ENABLES="	--host=arm-cortex-linux-gnueabi"
fi

[ ! -d objdir ] && mkdir -p objdir ;
cd objdir && ../configure \
	--build=x86_64-unknown-linux-gnu \
	--prefix=/usr \
	--exec-prefix=/usr \
	--bindir=/usr/bin \
	--sbindir=/usr/sbin \
	--sysconfdir=/etc \
	--datadir=/usr/share \
	--includedir=/usr/include \
	--libdir=/usr/lib \
	--libexecdir=/usr/libexec \
	--localstatedir=/var \
	--sharedstatedir=/usr/share \
	--mandir=/usr/share/man \
	--infodir=/usr/share/info \
	--with-tls \
	--with-__thread \
	$ARCHSPEC_ENABLES \
	--enable-shared \
	--build=x86_64-unknown-linux-gnu \
	--enable-kernel=2.6.32 \
	--without-cvs \
	--disable-profile \
	--without-gd \
	--with-headers=$KERNEL_H_INC_PATH

        #--disable-debug \

}

cleanLib() {

  confLib
  $pmake -r PARALLELMFLAGS="" CVSOPTS="" -C .. objdir=`pwd` distclean

}

buildLib() {

  # pre-build steps for arm
  if [ "`echo $ARCH | grep arm`" != "" ] ; then
    packagePorts
    [ -d ports ] && rm -rf ports
    tar xfj $GLIBC_PORTS_SRCFILE
    mv ${GLIBC_PORTS_TAG} ports
  fi
  confLib "-g"

  if [ "$1" != "" ] && [ "`echo $KNOWN_ARCHS | grep $1`" = "" ] ; then
    $pmake common-objpfx=../objdir/ objpfx=../objdir/$1/ objdir=$TOPDIR/objdir subdir=$1 -C $1 ..=../ all
  else
    $pmake $*
  fi
  RES=$?

  #  Make a host version of the zic program
  cp -r ${TOPDIR}/timezone host_timezone
  cd host_timezone
  ${BUILD_CC} -O2 -o zic ialloc.c scheck.c zic.c
}

buildAllTest() {

  cd objdir
  $pmake -r PARALLELMFLAGS="" CVSOPTS="" -C .. objdir=`pwd` check
  RES=$?

}

buildAll() {

  buildLib
  buildAllTest

}

buildTest() {

 confLib "-g"
 cd $TOPDIR
 [ "$1" = "elf" ] && objpfx=$TOPDIR/objdir/$1/
 [ "$1" != "" ] && [ "`echo $KNOWN_ARCHS | grep $1`" = "" ] && SUBDIR="subdir=$1 -C $1" && objpfx=../objdir/$1/
 $pmake common-objpfx=../objdir/ objpfx=$objpfx objdir=$TOPDIR/objdir $SUBDIR ..=../ tests
 RES=$?

}

installLib() {

[ ! -z "$1" -a "$1" != "$ARCH" ] && INSTALLROOT=$1 || INSTALLROOT=$TARGETROOT
echo -e "INSTALLING TO $INSTALLROOT.......\n"
cd objdir
$pmake install install_root=$INSTALLROOT
RES=$?

/usr/bin/install -c -m 644 $TOPDIR/objdir/libc.a $INSTALLROOT/usr/lib/libc.a

mkdir -p $INSTALLROOT/usr/share/man/man1
mkdir -p $INSTALLROOT/usr/share/man/man8
mkdir -p $INSTALLROOT/etc/init.d
mkdir -p $INSTALLROOT/etc/default

# This is only required because we are cross compiling - glibc will not do an install in this
# case as the zic we have just built as part of libc is a target executable.
cd host_timezone
# CRS - TODO Deal with systemv and backward which are links...
TZBASE="africa antarctica asia australasia europe northamerica southamerica etcetera factory solar87 solar88 solar89"
./zic -d $INSTALLROOT/usr/share/zoneinfo/right/ -L leapseconds -y yearistype $TZBASE
./zic -d $INSTALLROOT/usr/share/zoneinfo/posix/ -L /dev/null -y yearistype $TZBASE
./zic -d $INSTALLROOT/usr/share/zoneinfo/ -L /dev/null -y yearistype $TZBASE
cp $INSTALLROOT/usr/share/zoneinfo/America/New_York $INSTALLROOT/usr/share/zoneinfo/posixrules
cd ..

# Fix up libc.so and libpthread.so to use relative paths.
# libgcc_s.so doesn't appear to need this treatment.
for lib in libc libpthread ; do
  cat $INSTALLROOT/usr/lib/${lib}.so \
	| sed 's# /[^ ]*/lib# lib#g' \
	> $INSTALLROOT/usr/lib/${lib}.so.new
  mv -f $INSTALLROOT/usr/lib/${lib}.so.new \
	$INSTALLROOT/usr/lib/${lib}.so
done

mkdir -p $INSTALLROOT/etc
mv -f $INSTALLROOT/usr/share/locale/locale.alias $INSTALLROOT/etc/locale.alias
ln -s -f ../../../etc/locale.alias $INSTALLROOT/usr/share/locale/locale.alias

cd ..
mkdir -p $INSTALLROOT/usr/share/man/man3
/usr/bin/install -m 644 nscd/nscd.conf $INSTALLROOT/etc/nscd.conf
/usr/bin/install -m 644 nss/nsswitch.conf $INSTALLROOT/etc/
echo "order hosts,bind" > $INSTALLROOT/etc/host.conf

# MODIFIED : the following line has been changed to the one below it (path of nscd.init) :
#/usr/bin/install -m 644 debian/debhelper.in/nscd.init $INSTALLROOT/etc/nscd.conf
/usr/bin/install -m 755 nscd/nscd.init    $INSTALLROOT/etc/init.d/nscd

# Generate the ld.so.conf and ld.so.cache files
touch $INSTALLROOT/etc/ld.so.conf
touch $INSTALLROOT/etc/ld.so.cache

# build puts info in target/usr/info instead of target/usr/share/info for some as yet unknown reason...
if [ -d $INSTALLROOT/usr/info ] ; then
	mv -f $INSTALLROOT/usr/info $INSTALLROOT/usr/share/info
fi

# Remove files we don't package to keep RPM happy
rm -rf $INSTALLROOT/usr/share/info/dir*

if [ -d  $INSTALLROOT/usr/lib/pkgconfig ] ; then

list=`find $INSTALLROOT/usr/lib/pkgconfig -name "*.pc" -type f`
for i in $list ; do
sed -i -e 's,^exec_prefix=.*,exec_prefix=${prefix},' $i ;
sed -i -e '/^includedir=/s,/usr,${prefix},' -e  '/^libdir=/s,/usr,${exec_prefix},' $i ;
sed -i -e 's,'$INSTALLROOT'/usr/include,${includedir},g' $i ;
sed -i -e 's,'$INSTALLROOT'/usr/lib,${libdir},g' $i ;
done ;fi ;
if [ -d $INSTALLROOT/usr/share/man ] ; then

find $INSTALLROOT/usr/share/man "(" "(" -type f -o -type l ")" -a '!' -name "*.gz" ")" -print | while read AFile ; do
  test -f $AFile && gzip -9 -f $AFile;
  test -L $AFile && ln -s `readlink $AFile`.gz $AFile.gz && rm $AFile;
done ;fi ;
if [ -d  $INSTALLROOT/usr/share/info ] ; then

find $INSTALLROOT/usr/share/info "(" "(" -type f -o -type l ")" -a '!' -name "*.gz" ")" -print | while read AFile ; do
  test -f $AFile && gzip -9 -f $AFile;
  test -L $AFile && ln -s `readlink $AFile`.gz $AFile.gz && rm $AFile;
done ;rm -f  $INSTALLROOT/usr/share/info/dir.* ;
fi ;

[ -f $INSTALLROOT/usr/sbin/[zdump,rpcinfo] ] && mv -f $INSTALLROOT/usr/sbin/{zdump,rpcinfo} $INSTALLROOT/usr/bin/
# Don't provide /etc/rpc
rm -f ${INSTALLROOT}/etc/rpc

mkdir -p $INSTALLROOT/usr/lib/locale

}

packageItem() {

  TAG=$1
  TAR_OP="cj"
  ZIP_SFX=".bz2"
  PFX=`mktemp -d -p $PWD ./installed$TAG.XXX || exit 1`
  chmod 777 $PFX
  install$TAG $PFX
  TAG="_"$TAG
  [ "$2" = "PACKAGEALL" ] && TAR_OP="r" && TAG="" && ZIP_SFX=""
  tar $TAR_OP"vf" $CURRENT_GITTAG"_"$ARCH$TAG.tar$ZIP_SFX -C $PFX .
  [ -s $CURRENT_GITTAG"_"$ARCH$TAG.tar$ZIP_SFX ] && rm -rf $PFX
}

packageLib() {
  packageItem Lib
}

packagePorts() {

  pushd $TOPDIR
  pushd ${GLIBC_PORTS_PATH}
  make dist
  GLIBC_PORTS_TAG=`git describe --abbrev=0 | awk -F '-' '{print $2"-"$3"-"$4}'`
  GLIBC_PORTS_SRCFILE=$GLIBC_PORTS_TAG".tar.bz2"
  popd
  cp -f ${GLIBC_PORTS_PATH}/${GLIBC_PORTS_SRCFILE} .
}

packageSources() {

 confLib
 make dist
 make changelog
 cd $TOPDIR
 packagePorts
}

makeLibcConf() {
  [ "$1" != "" ] && PFX=$1 || PFX=$TARGETROOT
  CONFPFX=$PFX/etc
  mkdir -p $CONFPFX
  echo "libc:"$CURRENT_FULL_GITTAG > $CONFPFX/libc.conf
  set -- `${DEVKITROOT}/bin/$ARCH-linux-gcc --version 2>&1` && echo "cross-gcc:"${7/)/} >> $CONFPFX/libc.conf
  set -- `${DEVKITROOT}/bin/$ARCH-linux-ld -v 2>&1` && echo "cross-ld:"$8 >> $CONFPFX/libc.conf
}

packageTest() {
	packageItem Test
}

makeChangelog() {
  confLib
  make changelog
}

installTest() {

  # install test suite
  #--------------------------------------------------------
  [ "$1" != "" ] && PFX=$1 || PFX=$TARGETROOT
  TESTDIR=$PFX/usr/tests/$CURRENT_FULL_GITTAG/test

  mkdir -p $TESTDIR
  cp -fr test_runtime/* $TESTDIR

  $pmake -k -f test.mk TESTDIR=$TESTDIR PFX=$PFX tests-install

  makeLibcConf $1
  RES=$?

}

installAll() {
  installLib $1
  installTest $1
  RES=$?
}

packageAll() {

  packageSources
  export PREFIX=$PREFIX
  sudo -E $TOOL_NAME packageItem Lib PACKAGEALL
  #packageItem Lib PACKAGEALL
  packageItem Test PACKAGEALL
  bzip2 -f $CURRENT_GITTAG"_"$ARCH.tar
}

usage () {
  echo -e "\n $TOOL_NAME - $TOOL_VERSION $TOOL_VERS_DATE $COPYRIGHT"
  echo "$DESCRIPTION"
  echo -e "$USAGE\n"
}

check_param () {

  sed_cmd="sed -n -e /"`echo $OPT_LIST | sed -e s/\ /\\\/p\ -e\ \\\//g`"/p"
  opt_found=`echo $@ | $sed_cmd`
  [ "$opt_found" = "" -o "$#" = 0 -o "$*" = "buildTest" ] && usage && exit 1
}

TOOL_VERSION="1.0.1"
TOOL_VERS_DATE="20120626"
TOOL_NAME=$0
DESCRIPTION=" This tool is intended for compiling, installing and packaging both glibc libraries (sources) and tests."
COPYRIGHT="(Copyright STMicroelectronics 2010, 2011, 2012)"
KNOWN_ARCHS="sh4 armv5 armv7"
DEFAULT_ARCH=sh4

USAGE="\n Usage: $0 <option> [<param>] [<install_path>] [<arch>]\n
 Where <option> is either :
\t\t      buildLib          to cross-compile libraries
\t\t      buildTest <test>  to cross-compile test family named <test>
\t\t      buildAllTest      to cross-compile the whole testsuite
\t\t      buildAll          to cross-compile libraries and whole testsuite
\t\t      installLib        to install libraries (*)
\t\t      installTest       to install tests (*)
\t\t      installAll        to install libraries and tests (*)
\t\t      packageSources    to package sources (including ports)
\t\t      packagePorts      to package ports sources (arm, ...) only
\t\t      packageLib        to package libraries
\t\t      packageTest       to package tests
\t\t      packageAll        to package sources (including ports), libraries and tests
\t\t      makeLibcConf      to create the build configuration file
\t\t      makeChangelog     to create an incremental changelog for the current stable release
\t\t      cleanLib          to clean libraries
\t\t      usage             this help
\n Where <arch> is either `echo $KNOWN_ARCHS | sed 's/\ /\ |\ /g'` ($DEFAULT_ARCH) and must always be specified as last option in cmd line
\n A log file is generated for the given option to :
\t\t      <current_path>/<option>-<arch>.log
\n(*) Note that all install steps imply use of sudo command, so please check if you
    are among sudoers in current host and do not launch these steps in background
    as they implies user interactivity (sudo passwd prompt)
\n"

OPT_LIST="buildLib buildTest buildAll buildAllTest installLib installTest installAll cleanLib packageSources packageLib packagePorts packageTest packageAll makeLibcConf makeChangelog"
#set -x

arch_found=
for p in $* ; do
  [ "`echo $p | grep \"\-\"`" = "" ] && [ "`echo $KNOWN_ARCHS | grep \"$p\"`" != "" ] && ARCH=$p && param_list=${*#$ARCH*.} arch_found=1 && break
done

[ "$arch_found" = "" ] && ARCH=$DEFAULT_ARCH && param_list=$*

[ "$PREFIX" = "" ] && PREFIX="/stlinux/"`whoami`
DEVKITROOT=/opt/STM/STLinux-2.4/devkit/$ARCH
TARGETROOT=${PREFIX}${DEVKITROOT}/target
KERNEL_H_INC_PATH=$DEVKITROOT/target/usr/include
AUTOMAKE=$DEVKITROOT/../../host/share/automake-1.11
CURRENT_GITTAG=`git describe --abbrev=0  | awk -F '-' '{print $2"-"$3}'`
CURRENT_FULL_GITTAG=`git describe --abbrev=0  | awk -F 'stlinux2.4-' '{print $2}'`
pmake="make -j 12"
export PATH=$DEVKITROOT/bin:$PATH
umask 022

# The glibc root dir :
TOPDIR=$PWD
GLIBC_PORTS_PATH=$TOPDIR/../glibc-ports
check_param $param_list

if [ "$USER" != "root" -a "$USER" != "hudson" ]
then
 if [ "$1" = "installLib" ] || [ "$1" = "installTest" ] || [ "$1" = "installAll" ] || [ "$1" = "packageLib" ]
  then
   export PREFIX=$PREFIX
   sudo -E $0 $param_list $ARCH
   RES=$?
   exit $RES
 fi
fi
$param_list | tee $1-$ARCH.log 2>&1
echo "Done."
exit $RES
