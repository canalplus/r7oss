#!/bin/sh

#This script loads infra_test , create mss device files and if required , build user app 

module="infra_test"
msssink_dev="mss_sink"
msssrc_dev="mss_src"

# Remove module from kernel (just in case it is still running)
/sbin/rmmod $module

# Install module
modprobe $module

# Find major number used
major_sink=$(cat /proc/devices | grep $msssink_dev | awk '{print $1}')
echo Using major number $major_sink

major_src=$(cat /proc/devices | grep $msssrc_dev | awk '{print $1}')
echo Using major number $major_src

# Remove old device files
rm -f /dev/${msssink_dev}0
rm -f /dev/${msssrc_dev}0


# Ceate new device files
mknod /dev/${msssink_dev}0 c $major_sink 0
mknod /dev/${msssrc_dev}0  c $major_src  0

#mknod /dev/${mss_sink}0 c $major_sink 0
#mknod /dev/${mss_src}0 c $major_src 0

# Change access mode
chmod 666 /dev/${msssink_dev}0
chmod 666 /dev/${msssrc_dev}0

#build mss user app , if required
#cc mss_test_app.c -o mss_test_app.c
#if [ "$?" -eq "0" ] ; then
#./mss_test_app.o
#else
#echo "Error in MSS Test App"
#fi

