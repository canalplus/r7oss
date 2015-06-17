#!/bin/sh
export LC_ALL=C

md5mac() {
	if [ -x '/sbin/ip' ]
	then
		/sbin/ip link show eth0 | awk '/link\/ether/ {print $2}'
	elif [ -x '/sbin/ifconfig' ]
	then
		/sbin/ifconfig eth0 | awk '$1 == "eth0" {print $NF}'
	fi | md5sum | cut -f1 -d' '
}

initng_version() {
	if [ -e configure.ac ]
	then
		sed -ne '{s/[ \t]//g;s/^AC_INIT(\[initng\],\[\([0-9\.]*\|svn\)\])$/\1/p}' configure.ac
	else
		cat config.h | grep VERSION | awk '{ print $3 }'
	fi
}

svn_revision() {
	[ -e .svn ] && svn info | awk '$1 == "Revision:" { print $2 }' 2>/dev/null
}

UNAME="$(uname -o -r -n)"
INITNG_VERSION="$(initng_version)"
DISTRO="Unknown"
MD5MAC="$(md5mac)"
SVN="$(svn_revision)"

# Set the svn version of got it
[ "${SVN}" ] && INITNG_VERSION="${INITNG_VERSION}-${SVN}"

[ -e /etc/linspire-version ] && DISTRO="Linspire $(cat /etc/linspire-version)"
[ -e /etc/debian_version ] && DISTRO="Debian $(cat /etc/debian_version)"
[ -e /etc/gentoo-release ] && DISTRO="$(cat /etc/gentoo-release)"
[ -e /etc/pardus-release ] && DISTRO="Pardus $(cat /etc/pardus-release)"
[ -e /etc/fedora-release ] && DISTRO="Fedora $(cat /etc/fedora-release)"
[ -e /etc/pld-release ] && DISTRO="PLD $(cat /etc/pld-release)"
[ -e /etc/pingwinek-release ] && DISTRO="Pingwinek $(cat /etc/pingwinek-release)"
[ -e /etc/altlinux-release ] && DISTRO="Altlinux $(cat /etc/altlinux-release)"
[ -e /etc/mandriva-release ] && DISTRO="Mandriva $(cat /etc/mandriva-release)"
[ -e /etc/arch-release ] && DISTRO="Arch $(cat /etc/arch-release)"
[ -e /etc/slackware-version ] && DISTRO="Slackware $(cat /etc/slackware-version)"

cat <<EOF
Sending this data to the initng counter:
Uname:          "${UNAME}"
Initng_Version: "${INITNG_VERSION}"
Distro:         "${DISTRO}"
md5mac:         "${MD5MAC}"

Counting users and knowing what distros people are using
is extreamly important for us, to see if there is any intrest
for this product.

This script will use wget to send the data to
http://users.initng.org/ .
No more data then the above will be sent.

This does include the hardware address of yout ethernet
card eth0 in md5 encrypted form.

If this is a privacy problem for you, you should press CTRL-C NOW!
(and then disable count_me in autoconf or cmake)

You got 2 seconds to reject by pressing Ctrl + C
$(echo -ne "\a")
Please also go to http://users.initng.org/ and do
a manual register and leave comments about what you think.
EOF


sleep 2
echo "sending ... "
wget -o /dev/null -O- "http://users.initng.org/count.php?uname=${UNAME}&distro=${DISTRO}&initng_version=${INITNG_VERSION}&md5mac=${MD5MAC}"
echo
echo "Thank you for beeing counted."
exit 0
