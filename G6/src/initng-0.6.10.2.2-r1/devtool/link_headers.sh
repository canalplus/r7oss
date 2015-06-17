#!/bin/bash

#exit on first return error 
set -e
INITNG_DIR=`pwd`

#Install headers.
mkdir -p /usr/include/initng
for i in `find . -name *.h -printf "%f "`; do
    echo "${INITNG_DIR}/`find . -name $i -printf "%h/%f "` --> /usr/include/initng/$i"
    ln -s -f ${INITNG_DIR}/`find . -name $i -printf "%h/%f "` /usr/include/initng/$i
done
exit 0
exit 1
