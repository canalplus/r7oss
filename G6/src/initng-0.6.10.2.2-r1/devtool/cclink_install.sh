#!/bin/bash

#exit on first return error 
set -e
INITNG_DIR=`pwd`

# Uninstall as mutch as we can.
rm -r -f /sbin/initng /lib/initng /lib/libinitng* /sbin/ngc /sbin/ngdc
mkdir /lib/initng

cd ${INITNG_DIR}
for o in src plugins; do
    pushd $o
    make
    popd
done
cd ${INITNG_DIR}

for p in doc ; do
    pushd $p
    make install
    popd
done
cd ${INITNG_DIR}

ln -s -f ${INITNG_DIR}/src/initng /sbin/initng
ln -s -f ${INITNG_DIR}/devtool/test_parser /sbin/test_parser
ln -s -f ${INITNG_DIR}/src/libinitng.so /lib/libinitng.so
ln -s -f ${INITNG_DIR}/src/libinitng.so.0 /lib/libinitng.so.0
ln -s -f ${INITNG_DIR}/src/libinitng.so.0.0.0 /lib/libinitng.so.0.0.0

for i in `find plugins -name *.so -printf "%f "`; do
    echo "${INITNG_DIR}/`find plugins -name $i` --> /lib/initng/$i"
    ln -s -f ${INITNG_DIR}/`find plugins -name $i` /lib/initng/$i
done


#Install headers.
mkdir -p /usr/include/initng
for i in `find . -name *.h -printf "%f "`; do
    echo "${INITNG_DIR}/$i --> /usr/include/initng/$i"
    ln -s -f ${INITNG_DIR}/$i /usr/include/initng/$i
done

NGC4_LINK_SBIN="ngc ngdc ngstart ngstop ngzap ngrestart ngreboot nghalt ngstatus"
for i in $NGC4_LINK_SBIN; do
    echo "${INITNG_DIR}/plugins/ngc4/$i --> /sbin/$i"
    ln -s -f ${INITNG_DIR}/plugins/ngc4/ngc /sbin/$i
done


ln -s -f ${INITNG_DIR}/plugins/nge/nge /sbin/nge
ln -s -f ${INITNG_DIR}/plugins/nge/nge_raw /sbin/nge_raw
ln -s -f ${INITNG_DIR}/plugins/nge/ngde /sbin/ngde

mv /lib/initng/*client.so /lib
exit 0
exit 1
