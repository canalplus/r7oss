#!/bin/sh

cleanup() {
	cd ..
	rm -rf build
}

echo "**** Creating the build directory ****"
if [ -d build/ ] ; then
	rm -rf build
fi
mkdir build
cd build


if [ $? -eq 0 ] ; then
	echo "**** Configuring project ****"
	cmake ..
else
	cleanup
	exit 1
fi


if [ $? -eq 0 ] ; then
	echo "Configuration successful."
	echo "**** Compiling project ****"
	make
else
	cleanup
	exit 2
fi

if [ $? -eq 0 ] ; then
	echo "Compilation successful."
	echo "**** Installing project ****"
	if [ $UID -eq 0 ] ; then
		make install
	else	
		echo "root privileges required."
		su -c "make install"
	fi
else
	cleanup
	exit 3
fi

if [ $? -eq 0 ] ; then
	echo "Installation successful."
	echo "Reloading of initng is strongly recommended."
	echo "I can reload initng (via ngc -R) for you [yes|no]:"
	read answer
	if [ x$answer = x"yes" ] ; then
		echo "**** Reloading initng ****"
		if [ $UID -eq 0 ] ; then
			ngc -R
		else
			echo "root privileges required."
			su -c "/sbin/ngc -R"
		fi
	else
		echo "**** Skipping reloading of initng ****"
	fi
else
	cleanup
	exit 4
fi

if [ $? -eq 0 ] ; then
	# Calling count_me.sh from within the build directory (config.h)
	echo "Do you want to be counted as user of initng [yes|no]:"
	read answer
	if [ x$answer = x"yes" ] ; then
		../count_me.sh
	fi
	cleanup
else
	cleanup
	exit 5
fi
 
exit 0
