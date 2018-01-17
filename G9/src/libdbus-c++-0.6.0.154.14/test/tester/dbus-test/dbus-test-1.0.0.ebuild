# Copyright 2007 Wyplay
# $Header: $

inherit mercurial autotools cross-libtool eutils toolchain-funcs redist


DESCRIPTION="DBus C++ library"
HOMEPAGE="https://gitorious.org/dbus-cplusplus"
SRC_URI=""

LICENSE="LGPL-2.1"
PUBLIC=1

SLOT="0"
KEYWORDS="mips arm x86-intelce sh x86"
IUSE="debug tests"

DEPEND=">=dev-libs/libdbus-c++-0.6.0.150.24"
RDEPEND="virtual/libstdc++ >=sys-apps/dbus-1.1.1_p2"

src_compile () {
	eautoreconf || die "eautoreconf failed"

#	econf $(use_enable debug) \
#		  $(use_enable tests) ${myconf} || die "econf failed"

	econf --enable-debug || die "econf failed"

	emake || die "emake failed"
}


