Name:		%{_stm_pkg_prefix}-target-directfb-gfxdriver-stgfx2
Version:	@STM_BLITTER_VERSION@
Release:	1
License:	LGPL

URL:		http://www.stlinux.com
Source0:	stm-stgfx2-%{version}.tar.bz2

Buildroot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-%{_stm_target_arch}-XXXXXX)
BuildRequires:	%{_stm_pkg_prefix}-%{_stm_target_arch}-directfb-dev
Prefix:		%{_stm_cross_target_dir}

%define _docdir  %{_stm_cross_target_dir}%{_stm_target_doc_dir}
%define _pkgname %{_stm_pkg_prefix}-%{_stm_target_arch}-directfb-gfxdriver-stgfx2


Summary: STM DirectFB graphics driver stgfx2
Group: DirectFB/Libraries
%description
 STM's DirectFB graphics driver 'stgfx2' for hardware acceleration.


%package -n %{_pkgname}
Summary: STM DirectFB graphics driver stgfx2
Group: DirectFB/Libraries
Obsoletes: %{_stm_pkg_prefix}-%{_stm_target_arch}-stm-blitter-headers
%description -n %{_pkgname}
 STM's DirectFB graphics driver 'stgfx2' for hardware acceleration.
 .
 This package contains the actual driver. It supports the BDispII,
 as found in many ST SoCs.

%package -n %{_pkgname}-dbg
Summary: STM DirectFB graphics driver stgfx2 - debug info
Group: DirectFB/Development
Requires: %{_pkgname} = %{version}-%{release}
%description -n %{_pkgname}-dbg
 STM's DirectFB graphics driver 'stgfx2' for hardware acceleration.
 .
 This package provides debug information for stgfx2. Debug information
 is useful for providing meaningful backtraces in case of bugs.


%prep
%setup -qcTn stm-stgfx2-%{version} -a 0
%target_setup
_version_major=`echo %{version} | awk ' BEGIN { FS="." } { print $1 } '`
_version_minor=`echo %{version} | awk ' BEGIN { FS="." } { print $2 } '`
sed -e "s|AC_INIT.*|AC_INIT([stgfx2], [${_version_major}.${_version_minor}], [http://bugzilla.stlinux.com])|g" \
    -i configure.ac
sed -e "s/info->version.major =.*/info->version.major = ${_version_major};/g" \
    -e "s/info->version.minor =.*/info->version.minor = ${_version_minor};/g" \
    -i linux/directfb/gfxdrivers/stgfx2/stm_gfxdriver.c
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
# add -g for debug package
export CFLAGS="${CFLAGS} -g3"

%target_do_configure \
        --disable-static
%pmake


%install
%target_setup
%target_makeinstall_destdir
%target_install_fixup
# remove useless .la file
find %{buildroot}%{_stm_cross_target_dir}%{_stm_target_lib_dir} -name '*.la' -print0 | xargs -0 rm -rf
# pull debug from elf files out into separate files, to be packaged in the -dbg package
cd ..
: > debugfiles.list
files=`find %{buildroot}%{_stm_cross_target_dir}%{_stm_target_bin_dir} -type f` || true
files="${files} `find %{buildroot}%{_stm_cross_target_dir}%{_stm_target_lib_dir} -name '*so*' -type f | egrep -v '\.debug$' | egrep '(\.so$|\.so\.)'`"
for elffile in ${files} ; do
  sofile=`readelf -h ${elffile} 2> /dev/null | grep "DYN"` || true
  execfile=`readelf -h ${elffile} 2> /dev/null | grep "EXEC"` || true
  if [ "X${sofile}" != "X" -o "X${execfile}" != "X" ] ; then
    debugfile=%{_stm_cross_target_dir}%{_stm_target_debug_dir}`echo ${elffile} | sed -e s,%{buildroot}%{_stm_cross_target_dir},,`.debug
    mkdir -p `dirname %{buildroot}${debugfile}`
    %{_stm_target_toolprefix}objcopy --only-keep-debug ${elffile} %{buildroot}${debugfile}
    %{_stm_target_toolprefix}objcopy --strip-debug ${elffile}
    %{_stm_target_toolprefix}objcopy --add-gnu-debuglink=%{buildroot}${debugfile} ${elffile}
    echo ${debugfile} >> debugfiles.list
  fi
done


%clean
rm -rf %{buildroot}


%files -n %{_pkgname}
%defattr(-,root,root)
%{_stm_cross_target_dir}%{_stm_target_lib_dir}/directfb*/gfxdrivers/*.so
%doc AUTHORS COPYING

%files -n %{_pkgname}-dbg -f debugfiles.list
%defattr(-,root,root)
%doc AUTHORS COPYING


%changelog
* Mon Feb 10 2014 Amir NEIFAR <amir.neifar@st.com> - 1.32.0-1
- [Bugzilla: 38924]  Blitter - Add support of premultiplication for YCbCr source in blending operation
- [Bugzilla: 42227]  [BSKYB] BLit operation failure when one of the src is NV12
- [Bugzilla: 28335]  stgfx2 updates missing for YCbCr color handling

* Tue Jan 07 2014 Amir NEIFAR <amir.neifar@st.com> - 1.30.0-1
- [Bugzilla: 29010]  draw rect broken for planar pixel formats
- [Bugzilla: 34479]  [DT] Removal of runtime switch to non-DT
- [Bugzilla: 41391]  Disable color conversion with CLEAR porter duff
- [Bugzilla: 41711]  Fill color Inverted Red/Blue for BGR24 format
- [Bugzilla: 41202]  Blitter nodes way of programming gives bad transcode performances
- [Bugzilla: 39318]  SDK2 port on 3.10 - memory attributes and VM flags no longer supported

* Tue Oct 22 2013 Amir NEIFAR <amir.neifar@st.com> - 1.23.0-1
- [Bugzilla: 28703]  stgfx2: use AC_CHECK_DECLS to simplify check for DirectFB declarations
- [Bugzilla: 35666]  Fix compilation issue for bdisp_state_validate_CLIP()
- [Bugzilla: 24982]  Blitter Resize + Blend operation leave Artifacts
- [Bugzilla: 28407]  stgfx2: one Makefile per subdirectory is not needed

* Thu Aug 29 2013 Amir NEIFAR <amir.neifar@st.com> - 1.21.0-1
- [Bugzilla: 29597]  add stm_blitter_surface_set_color_index()
- [Bugzilla: 35641]  bdisp2_blit_simple_full should be static
- [Bugzilla: 35680]  Update version number used in RPM.
- [Bugzilla: 33227]  Driver needs update to claim support of 4k. 

* Fri Aug 23 2013 Amir NEIFAR <amir.neifar@st.com> - 1.20.0-1
- Update spec file to be inline with new naming convention.

* Thu Aug 22 2013 Amir NEIFAR <amir.neifar@st.com> - 1.19.2-1
- [Bugzilla: 35333] Fix coverity issues blitter & stgfx2

* Wed Aug 14 2013 Amir NEIFAR <amir.neifar@st.com> - 1.19.1-1
- [Bugzilla: 34745] user-space includes bpa2.h
- [Bugzilla: 34740] [DT] Avoid autoload by OF

* Thu Jul 18 2013 Amir NEIFAR <amir.neifar@st.com> - 1.19.0-1
- [Bugzilla: 31233]  [DT] Implement support of DT in driver

* Fri Jul 05 2013 Amir NEIFAR <amir.neifar@st.com> - 1.18.1-1
- [Bugzilla: 30667]   initial native DirectFB 1.7 support

* Wed Jun 26 2013 Amir NEIFAR <amir.neifar@st.com> - 1.18.0-1
- [Bugzilla: 29008] state setup needs rework
- [Bugzilla: 30663] stgfx2: include config.h where needed
- [Bugzilla: 30664] configure.ac: little clean up

* Wed Jun 19 2013 Amir NEIFAR <amir.neifar@st.com> - 1.17.1-1
- [Bugzilla: 32164] stgfx2: remove flicker filter flag from bdisp2_blitflags
- [Bugzilla: 31760] missing two pixels in the flicker filter result
- [Bugzilla: 31757] FF and CLUT format is not yet supported
- [Bugzilla: 32309] [SDK2.10] Fix failure for blend/xor with planar formats

* Wed Jun 12 2013 Amir NEIFAR <amir.neifar@st.com> - 1.17.0-2
- [Spec; Bugzilla: 31950] stgfx2 rpm should not 'own' DirectFB directories anymore

* Wed Jun 05 2013 Amir NEIFAR <amir.neifar@st.com> - 1.17.0-1
- [Bugzilla: 19023] Add support for flicker filter feature
- [Bugzilla: 20557] Add support for rotate YCbCr888 and AYCbCr8888 format
- [Bugzilla: 28331] missing check to prevent premultiply on YCbCr color (for fill)
- [Bugzilla: 30670] build: fix debug build
- [Bugzilla: 30665] configure.ac: little clean up wrt. CFLAGS

* Thu May 24 2013 Amir NEIFAR <amir.neifar@st.com> - 1.16.0-1
- [Bugzilla: 29605] Some time bdisp_running get reset while blitter is still running
- [Bugzilla: 29940] Bit-accuracy checks fail when using PreprocBlitter
- [Bugzilla: 29979] build: little configure.ac cleanup
- [Bugzilla: 30870] stgfx2: do parallel builds via sdk2 build system plug-in
- [Bugzilla: 30706] [CPS] Add new freeze and restore PM callbacks for stm_blitter

* Thu May 15 2013 Amir NEIFAR <amir.neifar@st.com> - 1.15.0-1
- [Bugzilla: 29619] implement colorspace handling (stgfx2)
- [Bugzilla: 29818] make sure no compiler warnings are ignored
- [Bugzilla: 29838] 1:1 LUT blits errnoeously enable CLUT conversion
- [Bugzilla: 29840] non-scaled blits always use slow path with STM_BLITTER_SBF_NO_FILTER
- [Bugzilla: 29780] incorrect colour fill for A1 destinations

* Thu Apr 25 2013 Amir NEIFAR <amir.neifar@st.com> - 1.14.1-1
- [Bugzilla: 28408] add STiH205/STiH207 support
- [Bugzilla: 29691] bdispII_aq_state: allow STM_BLITTER_SBF_NO_FILTER for planar destinations
- [Bugzilla: 29598] stih315: erroneous base address
- [Bugzilla: 28099] Implement h/w accelerated NV24 -> RGB stretchblits in stm-blitter and stgfx2
- [Bugzilla: 28701] implement destination colour keying (draw)

* Wed Apr 17 2013 Akram Ben Belgacem <akram.ben-belgacem@st.com> - 1.14.0-1
- [Bugzilla: 28212] most checkpatch warnings
- [Bugzilla: 28218] some refatoring (separate out real h/w blitstate from other stuff)
- [Bugzilla: 28220] incorrect check for planar format hw support
- [Bugzilla: 28690] ALUT88 is an indexed format!
- [Bugzilla: 28290] support native porter duff blends (for drawing)
- [Bugzilla: 28292] support destination premultiplication (for drawing)
- [Bugzilla: 28662] [STiH407] add support for H407
- [Bugzilla: 28693] supported directfb draw and blit flags not updated based on hw features
- [Bugzilla: 28701] implement destination colour keying (draw)
- [Bugzilla: 28781] stgfx2: sdk2build.mak variables must be prefixed
- [Bugzilla: 28849] index colors need rework
- [Bugzilla: 29102] fix bash'ism in build_module.sh
- [Bugzilla: 29162] assertion on unknown pixel formats
- [Bugzilla: 29187] Porter/Duff CLEAR broken for YCbCr destinations (draw)
- [Bugzilla: 29191] Porter/Duff blend broken for all RGB surface formats (draw)
- [Bugzilla: 29350] Chroma is not ok for 3plane destination formats
- [Bugzilla: 29409] filter tap start position incorrect during rescale operations
- [Bugzilla: 29410] implement STM_BLITTER_SBF_NO_FILTER and DirectFB's DSRO_SMOOTH_UPSCALE / DSRO_SMOOTH_DOWNSCALE

* Fri Mar 29 2013 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.13.4-1
- [Bugzilla: 28874] should do backwards blit when source.x == dest.x
- [Bugzilla: 26881] Bliting from a 2 Plane surface to 3 Plane surface
- [Bugzilla: 28235] native support for RGB32
- [Bugzilla: 23612] remove checks for 64MB banks
- [Bugzilla: 28223] support larger sizes (4k) for blit operations
- [Bugzilla: 28222] rework device info for new hw features

* Tue Mar 19 2013 Akram Ben Belgacem <akram.ben-belgacem@st.com> - 1.13.3-1
- [Bugzilla: 28300] latest blitter driver causes crash on all pre-Orly2 IPs and memory corruption on all IPs

* Wed Mar 13 2013 Akram Ben Belgacem <akram.ben-belgacem@st.com> - 1.13.2-1
- [Bugzilla: 25803] stgfx2: StretchBlit + Clip is not pixel accurate
- [Bugzilla: 28075] refactor device info
- [Bugzilla: 28210] update stgfx2 version number automatically

* Fri Mar 08 2013 Akram Ben Belgacem <akram.ben-belgacem@st.com> - 1.13.1-1
- [Bugzilla: 24494] Blitter hang-up when running twice 'modprobe encoderstkpi'
- [Bugzilla: 25926] some cleanup patches
- [Bugzilla: 27585] "make clean" should remove objdir.armv7 directory
- [Bugzilla: 27943] stgfx2: add basic support for new DirectFB pixelformats
- [Bugzilla: 27976] incorrect check for NVxx read vs. write support
- [Bugzilla: 28027] stgfx2: missing initialisation of new stuff

* Fri Feb 22 2013 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.13.0-1
- Add usage of proot for autogen tools.

* Thu Jan 31 2013 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.12.3-1
- [Bugzilla: 26577] stgfx2: possible memory leak in bdisp_ap_StateInit()

* Thu Jan 17 2013 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.12.2-1
- [Bugzilla: 25624] latest DirectFB acceleration gives incorrect results
- [Bugzilla: 23082] compilation script for build environment

* Sun Jan 13 2013 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.12.1-1
- [Bugzilla: 25690] mock-build: fix template phony hang in build system

* Wed Dec 19 2012 Akram Ben Belgacem <akram.ben-belgacem@st.com> - 1.12.0-1
- [Bugzilla: 21981] Blending operation shows some color mismatch
- [Bugzilla: 23603] stm_blitter_wait : cpu performances
- [Bugzilla: 24896] compilation script for build environment
- blitter: STiH416 SW fix for MacroBlock Source HW limitation

* Thu Nov 29 2012 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.11.0-1
- [Bugzilla: 23607] add platform device information for Orly2 (STiH416)
- [Bugzilla: 24643] Support of user space compilation using the sdk2build.mak mechanism

* Thu Nov 08 2012 Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com> - 1.10.4-1
- stgfx2: remove wrong include.
- [Bugzilla: 23325] Blitter header file is not OS agnostic
- [Bugzilla: 20167] add support for complex blits to planar destination formats
- [Bugzilla: 21998] Format Conversion between UYVY and YUYV(YUY2)

* Mon Oct 22 2012 Akram Ben Belgacem <akram.ben-belgacem@st.com> - 1.10.2-1
- [Bugzilla: 23298] recent versions of stgfx2 have lost compatibility with old versions of stmfb
- stgfx2: work around an error in DirectFB 1.6

* Thu Sep 06 2012 André Draszik <andre.draszik@st.com> - 1.10.0-1
- [Bugzilla: 16581] add support for active standby

* Fri Aug 17 2012 André Draszik <andre.draszik@st.com> - 1.9.1-2
- [Spec] bump release number to force rebuild against DirectFB 1.6

* Fri Aug 03 2012 André Draszik <andre.draszik@st.com> - 1.9.0-1
- follow device name change from 0 to -1 if we only have one IP in the system
- add new DirectFB 1.6 pixelformats LUT4 ALUT8 NV12MB NV16MB BGR24
- fix compilation if DirectFB has no fixed point support (incomplete in 1.4.0-1)

* Tue Jul 03 2012 André Draszik <andre.draszik@st.com> - 1.8.0-1
- build: fix warnings caused by gcc's -Wsign-compare
- [Bugzilla: 20206] remove info message for blitter type
- [Bugzilla: 20128] add simple copy support for NV12 NV21 NV16 NV61 YV12 I420 YV16 YV61 YUV444P
  destinations

* Fri Jun 22 2012 André Draszik <andre.draszik@st.com> - 1.6.0-1
- [Bugzilla: 20071] debug updates

* Wed Jun 20 2012 André Draszik <andre.draszik@st.com> - 1.5.0-1
- [Bugzilla: 19696] bdisp2_blit2() broken for non fixed point coordinates

* Tue May 29 2012 André Draszik <andre.draszik@st.com> - 1.4.1-1
- [Bugzilla: 18187] stgfx2: better error message if stm-blitter is not loaded

* Fri Mar 30 2012 André Draszik <andre.draszik@st.com> - 1.4.0-1
- [Bugzilla: 18025] add support for DirectFB 1.6 in stgfx2 graphics driver

* Fri Dec 16 2011 André Draszik <andre.draszik@st.com> - 1.1.3-1
- [Spec] have the -dbg package depend on the normal package
- for further changes, see the changelog of host-stm-blitter-source

* Mon Nov 28 2011 André Draszik <andre.draszik@st.com> - 1.1.1-1
- [Bugzilla: 15549] Make stgfx2 use BDisp1 (instead of BDisp0) where
  available

* Fri Nov 25 2011 André Draszik <andre.draszik@st.com> - 1.1.0-1
- [Bugzilla: 15510] split stgfx2 out from DirectFB into own package
- [Bugzilla: 15510] rewrite stgfx2 to work in multiple environments
