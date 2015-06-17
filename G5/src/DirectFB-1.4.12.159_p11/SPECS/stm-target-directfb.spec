%define _dfb_build_config	x@BUILD_CONFIG@

Name:		%{_stm_pkg_prefix}-target-directfb@BUILD_CONFIG@
Summary:	Hardware graphics acceleration library
Group:		DirectFB/Libraries
%define _dfbversion	1.4.12
%define _abiversion	1.4-5
%define _stmversion	+STM2011.04.14
Version:	%{_dfbversion}%{_stmversion}
Release:	1
Epoch: 1
License:	LGPL
# created by:
# git archive --format=tar --prefix=DirectFB-1.4.12+STM2011.04.14/ DIRECTFB_1_4_12_STM2011.04.14 | bzip2 --best > DirectFB-1.4.12+STM2011.04.14.tar.bz2
Source0:	DirectFB-%{version}.tar.bz2

URL:		http://www.directfb.org
Buildroot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-%{_stm_target_arch}-XXXXXX)
Prefix:		%{_stm_cross_target_dir}

%if_target_cpu sh
BuildRequires:	%{_stm_pkg_prefix}-%{_stm_target_arch}-stmfb-headers >= 3.1_stm24_0101
%define _dfb_gfxdrivers stgfx
%define _mme_enabled yes
%else
%define _dfb_gfxdrivers none
%define _mme_enabled no
%endif
%if "%{_dfb_build_config}" == "x-multi"
BuildRequires: %{_stm_pkg_prefix}-%{_stm_target_arch}-linux-fusion-headers >= 8.2.0
%define _dfb_multiapp --enable-multi
%else
%define _dfb_multiapp --disable-multi
%endif

%define _pkgname	%{_stm_pkg_prefix}-%{_stm_target_arch}-directfb
%define _fullname	directfb-%{_abiversion}
%define _docdir		%{_stm_cross_target_dir}%{_stm_target_doc_dir}


#
#  SRPM Package
#
%description
The source package for directfb.

#
#  RPMS
#
%package -n %{_pkgname}@BUILD_CONFIG@
Summary:	Hardware graphics acceleration library
Group:		DirectFB/Libraries
Provides:	%{_pkgname} = %{version}-%{release}
%if "%{_dfb_build_config}" == "x-multi"
Provides:	%{_pkgname}-multi = %{version}-%{release}
Conflicts:	%{_pkgname}-single
%else
Provides:	%{_pkgname}-single = %{version}-%{release}
Conflicts:	%{_pkgname}-multi
%endif
%description -n %{_pkgname}@BUILD_CONFIG@
DirectFB is a thin library that provides developers with hardware graphics
acceleration, input device handling and abstraction, an integrated windowing
system with support for translucent windows and multiple display layers on top
of the Linux frame buffer device. It is a complete hardware abstraction layer
with software fallbacks for every graphics operation that is not supported by
the underlying hardware.
%if "%{_dfb_build_config}" == "x-multi"
This version has been built with the Multi Application Core.
%else
This version has been built with the Single Application Core.
%endif

%package -n %{_pkgname}@BUILD_CONFIG@-dev
Summary:	Hardware graphics acceleration library - development
Group:		DirectFB/Development
AutoReq:	no
Provides:	%{_pkgname}-dev = %{version}-%{release}
%if "%{_dfb_build_config}" == "x-multi"
Requires:	%{_pkgname}-multi = %{version}-%{release}
Provides:	%{_pkgname}-multi-dev = %{version}-%{release}
%else
Requires:	%{_pkgname}-single = %{version}-%{release}
Provides:	%{_pkgname}-single-dev = %{version}-%{release}
%endif
%description -n %{_pkgname}@BUILD_CONFIG@-dev
DirectFB header files needed for building DirectFB applications.

%package -n %{_pkgname}@BUILD_CONFIG@-dbg
Summary:	Hardware graphics acceleration library - debug info
Group:		DirectFB/Development
AutoReq:	no
Provides:	%{_pkgname}-dbg = %{version}-%{release}
%if "%{_dfb_build_config}" == "x-multi"
Requires:	%{_pkgname}-multi = %{version}-%{release}
Provides:	%{_pkgname}-multi-dbg = %{version}-%{release}
%else
Requires:	%{_pkgname}-single = %{version}-%{release}
Provides:	%{_pkgname}-single-dbg = %{version}-%{release}
%endif
%description -n %{_pkgname}@BUILD_CONFIG@-dbg
This package provides debug information for DirectFB. Debug information
is useful for providing meaningful backtraces in case of bugs.


%prep
%target_setup
%setup -q -n DirectFB-%{version}
%target_autoreconf

%build
%target_setup
# the st231 compiler emits a warning when it encounters multiple -O statements,
# which makes auto* assume that some of its tests failed. Strip out existing -O
# and add -O3
_stripped_flags=
for _this_flag in $CFLAGS ; do
  _stripped_flags="${_stripped_flags} `echo $_this_flag | sed -e 's,-O.,,'`"
done
export CFLAGS="${_stripped_flags} -O3"
export CPPFLAGS="$CPPFLAGS -DDIRECTFB_VERSION_VENDOR=\\\"%{_stmversion}\\\""
# add -g for debug package
export CFLAGS="${CFLAGS} -g3"

%target_do_configure \
	--enable-static \
	\
	--disable-devmem \
	--disable-sdl \
	\
	--with-gfxdrivers=%{_dfb_gfxdrivers} \
	--enable-mme=%{_mme_enabled} \
	\
	%{_dfb_multiapp}
%pmake


%install
%target_setup
%target_makeinstall_destdir

# build directfb-config for host environment
# we assume that _stm_target_lib_dir is in the default search path of both the cross
# and target dynamic linkers, to suppres spurious -L/usr/lib in *.la files
mkdir -p %{buildroot}%{_stm_cross_bin_dir}
sed -e "s,libs=-L%{_stm_target_lib_dir},libs=,g" \
    -e "s,%{_stm_target_prefix},%{_stm_cross_target_dir}%{_stm_target_prefix},g" \
    < %{buildroot}%{_stm_cross_target_dir}%{_stm_target_bin_dir}/directfb-config \
    > %{buildroot}%{_stm_cross_bin_dir}/%{_stm_target_toolprefix}directfb-config
chmod +x %{buildroot}%{_stm_cross_bin_dir}/%{_stm_target_toolprefix}directfb-config

%target_install_fixup
# Process target .pc files so they are useful in a cross environment
for f in %{buildroot}%{_stm_cross_target_dir}%{_stm_target_pkgconfig_dir}/*.pc ; do
  sed -i '/^prefix=/!s,%{_stm_target_prefix},${prefix},' $f
done


cd ..
cp COPYING LICENSE


# pull debug from solibs out into separate files, to be packaged in the -dbg package
# have to be careful to not include files like libdirectfb_sonypi.la or shell scripts
rm -f dfb-debuginfo.filelist
files=`find %{buildroot}%{_stm_cross_target_dir}%{_stm_target_bin_dir} -type f | grep -v directfb-config`
files="${files} `find %{buildroot}%{_stm_cross_target_dir}%{_stm_target_lib_dir} -name 'lib*so*' -type f | egrep -v '\.debug$' | egrep '(\.so$|\.so\.)'`"
for solib in ${files} ; do
  debugfile=%{_stm_cross_target_dir}%{_stm_target_debug_dir}`echo ${solib} | sed -e s,%{buildroot}%{_stm_cross_target_dir},,`.debug
  mkdir -p `dirname %{buildroot}${debugfile}`
  %{_stm_target_toolprefix}objcopy --only-keep-debug ${solib} %{buildroot}${debugfile}
  %{_stm_target_toolprefix}objcopy --strip-debug ${solib}
  %{_stm_target_toolprefix}objcopy --add-gnu-debuglink=%{buildroot}${debugfile} ${solib}
  echo ${debugfile} >> dfb-debuginfo.filelist
done


%clean
rm -rf %{buildroot}


%files -n %{_pkgname}@BUILD_CONFIG@
%defattr(-,root,root)
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/lib*.so.*
%dir %{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}
%dir %{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/*
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/*/lib*.so
%dir %{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/interfaces/*
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/interfaces/*/lib*.so
%dir %{_stm_cross_target_dir}%{_stm_target_data_dir}/directfb-%{_dfbversion}
%{_stm_cross_target_dir}%{_stm_target_data_dir}/directfb-%{_dfbversion}/cursor.dat
%{_stm_cross_target_dir}%{_stm_target_data_dir}/man/man5/directfbrc*5*
%doc AUTHORS ChangeLog LICENSE NEWS README TODO

%files -n %{_pkgname}@BUILD_CONFIG@-dev
%defattr(-,root,root)
%{_stm_cross_target_dir}%{_stm_target_bin_dir}/*
%{_stm_cross_target_dir}%{_stm_target_include_dir}/directfb
%{_stm_cross_target_dir}%{_stm_target_include_dir}/directfb-internal
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/lib*.so
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/lib*.a
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/lib*.la
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/*/lib*.a
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/*/lib*.la
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/*/lib*.o
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/interfaces/*/lib*.a
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/interfaces/*/lib*.la
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/%{_fullname}/interfaces/*/lib*.o
%{_stm_cross_target_dir}%{_stm_target_pkgconfig_dir}/*.pc
%{_stm_cross_target_dir}%{_stm_target_data_dir}/man/man1/dfbg*1*
%{_stm_cross_target_dir}%{_stm_target_data_dir}/man/man1/directfb-csource*1*
%{_stm_cross_bin_dir}/*
%doc AUTHORS ChangeLog LICENSE NEWS README TODO

%files -n %{_pkgname}@BUILD_CONFIG@-dbg -f dfb-debuginfo.filelist
%defattr(-,root,root)
%doc AUTHORS ChangeLog LICENSE NEWS README TODO


%changelog
* Thu Apr 14 2011 André Draszik <andre.draszik@st.com> 1:1.4.12+STM2011.04.14-1
- [Update: 1.4.12+STM2011.04.14] update to DirectFB 1.4.12 and latest STM version
- [Bugzilla: 11689] inputdrivers: support lirc>=0.8.6
- [Bugzilla: 11884] build: libidirectfbfont_ft2 must be linked against libm
- [Delete patch: DirectFB-1.4.11-non-mme-hotfix.patch] not needed anymore
- [Bugzilla: 11922] fix raw jpeg decoding for libjpeg >= v7

* Tue Apr 05 2011 André Draszik <andre.draszik@st.com> 1:1.4.11+STM2010.12.15-4
- [Bugzilla: 11825; Spec] add debug info package
- [Spec] update summary of -dev package

* Thu Jan 06 2011 André Draszik <andre.draszik@st.com> 1:1.4.11+STM2010.12.15-3
- [Add patch: DirectFB-1.4.11-non-mme-hotfix.patch] hotfix for builds with
  disabled MME
- Breaks binary compatibility with Mali drivers (Mali drivers need to be rebuilt)

* Wed Jan 05 2011 André Draszik <andre.draszik@st.com> 1:1.4.11+STM2010.12.15-2
- [Spec] disable MME use for image decoding for non sh4 builds during configure
  to fix build failures on ARM

* Wed Dec 15 2010 André Draszik <andre.draszik@st.com> 1:1.4.11+STM2010.12.15-1
- [Update: 1.4.11+STM2010.12.15] new release based on 1.4.11 + git 811a8c0
- [Bugzilla: 10320] don't set any output resolution on startup
- [Bugzilla: 10344] take DirectFB's init-layer=<x> option into account
- [Bugzilla: 10769] want SOURCE2 support in DirectFB BDisp driver
- image providers: accelerate JPEG and PNG using MME
- fixed point fixes
- [Delete patch: DirectFB-1.4.3-0001-stgfx2-version-bump.patch] integrated upstream
- [Spec] Bump BuildRequires for linux-fusion to 8.2.0
- [Spec] drop setting of LIBPNG_CONFIG - it's not needed anymore

* Thu Oct 21 2010 André Draszik <andre.draszik@st.com> 1:1.4.3+STM2010.10.15-1
- [Add patch: DirectFB-1.4.3-0001-stgfx2-version-bump.patch] bump stgfx2 version
  to 0.8

* Fri Oct 15 2010 André Draszik <andre.draszik@st.com> 1:1.4.3+STM2010.10.15-1
- [Update: 1.4.3+STM2010.10.15] new release
- [Bugzilla: 10253] jpeg: fix raw decode error paths
- [Bugzilla: 10242] stgfx2: DSBLIT_XOR does not work
- [Bugzilla: 10254] stgfx2: incorrect use of line filters for 'slim' stretch blits
- [Bugzilla: 10228] stgfx2: disable use of hw based clipping
- [Bugzilla: 10226] stgfx2: full DSBLIT_BLEND_COLORALPHA support
- [Bugzilla: 10227] stgfx2: fix some 'unusual' PorterDuff blends
- [Bugzilaa: 10247] stgfx2: DSPD_CLEAR crashes the BDisp in 422r modes
- stgfx2 cleanup: blit state rewrite, compiler warnings, debug for DirectFB 1.4.3,
  fixes for RGB32
- [Spec] some changes so as to make future updates easier

* Thu Aug 26 2010 André Draszik <andre.draszik@st.com> 1:1.4.3+STM2010.06.22-2
- [Spec] fix build requires to reference correct stmfb headers package version
  on sh4

* Wed Aug 25 2010 André Draszik <andre.draszik@st.com> 1:1.4.3+STM2010.06.22-1
- [Update: 1.4.3+STM2010.06.22] new release
- [Delete patch: DirectFB-1.4.3+STM2010.06.16-stmfb0029.patch] we have an up to
  date stmfb in STLinux 2.4 now, so this patch is a) harmful for STi7108 support
  and b) not needed anymore anyway
- stgfx2: fix clip validation

* Wed Jun 16 2010 André Draszik <andre.draszik@st.com> 1:1.4.3+STM2010.06.16-1
- [Update: 1.4.3.STM2010.06.16] new release
- [Delete patch: DirectFB-1.4.3.STM2010.03.10-libjpeg7.patch, 
   DirectFB-1.4.3.STM2010.03.10-libjpeg7_2.patch]
  jpeg problems correctly fixed upstream
- [Delete patch: DirectFB-1.4-Revert-versioned-DirectFB-libraries.patch]
  not needed anymore
- [Add patch: DirectFB-1.4.3+STM2010.06.16-stmfb0029.patch] needed now

* Tue Apr 27 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.03.10-4
- [Bugzilla: 8912; Add patch: DirectFB-1.4.3.STM2010.03.10-libjpeg7.patch,
   DirectFB-1.4.3.STM2010.03.10-libjpeg7_2.patch]
  fix libjpeg usage for libjpeg versions >= 7
- [Spec] simplify file ownership list
- [Spec] bump version to rebuild against updated libjpeg

* Tue Apr 13 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.03.10-3
- [Spec] fix source package name

* Wed Mar 31 2010 Stuart Menefy <stuart.menefy@st.com> 2
- [Spec] Bump the release number for 2.4 product release.
- [Spec] Update BuildRoot to use %%(mktemp ...) to guarantee a unique name.

* Wed Mar 09 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.03.10-1
- [Spec] add some more files to main package's documentation
- merge in latest updates from DirectFB/master branch
- input: handle button devices with just up/down keys as keyboards, too
- stgfx2: fix shutdown error path
- stgfx2: big cleanup regarding the destination address
- stgfx2: slightly smaller cleanup regarding the s3 address & type
- stgfx2: cleanup regarding s2 address & type
- stgfx2: huge cleanup regarding drawing state
- the above four changes yield in about 10% less CPU usage on fills!
- [Bugzilla: 8256] XOR doesn't work as expected
- [Bugzilla: 8406] prevent BDisp crash when doing YCbCr422R fast blit
- [Bugzilla: 8366] desaturation of YCbCr surfaces
- stgfx2: back to bdisp_aq_VideoYCbCr601_2_RGB matrix for YCbCr->RGB conversions
- stgfx2: allow 'other' accelerators to access our surface pools
- stgfx2: RGB32 updates

* Fri Feb 05 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.02.08-1
- [Bugzilla: 8193] do necessary changes (pollvsync_after)
- [Bugzilla: 7360, 8077] subpixel vertical filter setup has been
  greatly simplified and corrected

* Fri Feb 05 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.02.05-1
- stgfx stgfx2: fix shutdown when not running in stmfbdev system
- linux_input: fix compilation if stmfbdev is disabled

* Sun Jan 31 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.01.31-1
- merge in latest updates from DirectFB/master branch
- stmfbdev: address compiler warnings in non debug builds
- stmfbdev: fix crash in shutdown
- stmfbdev: optimize ioctl handling
- stmfbdev: don't instanciate screens, layers & surface pools anymore
- stgfx/stgfx2: move instanciation of the above here, this gets us rid of more of DirectFB's startup warnings

* Fri Jan 29 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2010.01.29-1
- [Bugzilla: 7995] new system: 'stmfbdev' for many new features reg. screen control
- stgfx2:
  + some fixes for blit and fill if destination is YUV
  + WA for https://bugzilla.stlinux.com/show_bug.cgi?id=7084
  + update alignment restrictions for STi7108
  + stgfx surface pools: prevent confusing startup message
- stgfx:
  + update alignment restrictions for STi7108
  + stgfx surface pools: prevent confusing startup message
- misc:
  + dfbscreen: little fixes
  + generic: LUT8 is not a YUV format
  + screen: NTSC and PAL60 standards are defined as 59.94Hz not 60Hz
  + IDirectFBScreen: add IDirectFBScreen::GetVSyncCount()
  + directfb.h: add a DVO 656 'connector'
  + dfblayer: allow to set layer position and size using command line
  + dfbinspector: add DSPF_AVYU and DSPF_VYU pixelformats
- [Delete patch: DirectFB-1.4.3.STM2009.12.11-CLUT8_fix.patch] integrated upstream

* Fri Jan 29 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2009.12.11-4
- [Add patch: DirectFB-1.4.3.STM2009.12.11-CLUT8_fix.patch] fix CLUT8 issue in
  software renderer

* Mon Jan 11 2010 André Draszik <andre.draszik@st.com> 1.4.3.STM2009.12.11-3
- [Spec] unify multi app and single app spec files

* Wed Dec 16 2009 André Draszik <andre.draszik@st.com> 1.4.3.STM2009.12.11-2
- [Add patch: DirectFB-1.4.3.STM2009.12.11-autoconf259.patch] so as to enable
  successful build on arm with old autotools in STLinux-2.3

* Fri Dec 11 2009 André Draszik <andre.draszik@st.com> 1.4.3.STM2009.12.11-1
- [update: 2009-12.11] update to DirectFB 1.4.3
- misc: use pkgconfig to detect X11
- misc: surface: replace GetFramebufferOffset() with GetPhysicalAddress()
- stgfx2: pick up all updates from 1.0.1 branch
- stgfx2: release surface pools on shutdown
- [Bugzilla: 7776] stgfx2: cleanup draw/blitting wrt flags
- stgfx: release surface pools on shutdown
- [Bugzilla: 7570] jpeg: implement decimated decode for software JPEG decoder
- [Bugzilla: 7636] jpeg: implement raw libjpeg decode for possible HW acceleration (using stgfx2)

* Fri Aug 21 2009 André Draszik <andre.draszik@st.com> 1.4.1.STM2009.08.21-1
- [update: 2009-08-21]
  allow source widths and heights < 1.0 (but > 0) in BatchStretchBlit()

* Tue Aug 18 2009 André Draszik <andre.draszik@st.com> 1.4.1.STM2009.08.18-1
- [update: 2009-08-18]

* Tue Jul 29 2009 André Draszik <andre.draszik@st.com> 1
- [update: 1.4.1] new upstream version with all the STM patches
