#[player2]
#'BUILD_DIR=player2
#'EXCLUDE_ALL="*~ *.orig *.rej"
##
#[end]

%define _int_release 191
%define _mtn_revision PLAYER2_INT_%{_int_release}

Summary:	STMicroelectronics' Player2 media playback engine
Name:		%{_stm_pkg_prefix}-player2
Version:	int%{_int_release}
Release:	1
URL:		http://www.stlinux.com
License:	GPL
Group:		Development/System
Buildroot:	%{_tmppath}/%{name}-%{version}-root
Prefix:		%{_stm_install_prefix}
Source:         player2.tar.bz2

%description
Player 2 is a media playback engine for digital video recorders, media 
centres, HD optical disc (both BD-ROM and HD-DVD) and A/V receiver 
applications.

Player 2 is supplied with wrappers that implement standard Linux
interfaces to control its operation. This primarily include LinuxDVB,
ALSA and Video4Linux2 augmented with sysfs controls for notifications
and management that fall outside the scope of the 'standard' media
interfaces.

%ifarch noarch

%define _pkgname %{_stm_pkg_prefix}-host-player2-source

%package -n %{_pkgname}
Summary          : STMicroelectronics' Player2 media playback engine
Group            : Development/System
Requires         : %{_stm_pkg_prefix}-host-filesystem
Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-filesystem
#Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-ACF_firmware >= BL021_06-1
#Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-ACF_firmware-dev >= BL021_06-1
#Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-vid_firmware-dev
#Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-multicom-devel

%description -n %{_pkgname}
The sources to Player2, a media playback engine for digital video 
recorders, media centres, HD optical disc (both BD-ROM and HD-DVD) and 
A/V receiver applications.

Player 2 is supplied with wrappers that implement standard Linux
interfaces to control its operation. This primarily include LinuxDVB,
ALSA and Video4Linux2 augmented with sysfs controls for notifications
and management that fall outside the scope of the 'standard' media
interfaces.

%else

%define _pkgname %{_stm_pkg_prefix}-%{_stm_target_arch}-player2

%package -n %{_pkgname}
Summary          : STMicroelectronics' Player2 media playback engine
Group            : Development/System
Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-filesystem

%description -n %{_pkgname}
Test applications for Player2, a media playback engine for digital video 
recorders, media centres, HD optical disc (both BD-ROM and HD-DVD) and 
A/V receiver applications.

Player 2 is supplied with wrappers that implement standard Linux
interfaces to control its operation. This primarily include LinuxDVB,
ALSA and Video4Linux2 augmented with sysfs controls for notifications
and management that fall outside the scope of the 'standard' media
interfaces.

%package -n %{_pkgname}-devel
Summary          : Player2 header files and documentation for development %{_stm_target_arch}
Group            : Development/System
Requires         : %{_stm_pkg_prefix}-%{_stm_target_arch}-filesystem

%description -n %{_pkgname}-devel
Player2 header files and documentation for %{_stm_target_arch} target.

%endif

%prep

rm -rf stgfb-%{version}*
%setup -qn player2-%{version} -c player2-%{version}

%build
%target_setup

# Update the project version number
mv Doxyfile Doxyfile.orig
sed -e '/^PROJECT_NUMBER/s/UNVERSIONED/%{_mtn_revision}/' < Doxyfile.orig > Doxyfile
rm Doxyfile.orig

%ifarch noarch
%else
# Generate the documentation
make ARCH=sh4 doxygen
%endif

%install
rm -rf %{buildroot}

%ifarch noarch
install -d -m 755 %{buildroot}%{_stm_sources_dir}/player2/player2-%{version}-%{release}
tar chf - . | tar xf - -C %{buildroot}%{_stm_sources_dir}/player2/player2-%{version}-%{release}

%else
mkdir -p %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_bin_dir}

mkdir -p %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_doc_dir}/%{_pkgname}
mkdir -p %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_include_dir}/linux

cp -dprf linux/include/linux/* %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_include_dir}/linux/
cp -dprf linux/drivers/media/dvb/stm/dvb/dvb_avr_export.h %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_include_dir}/linux/
cp -dprf linux/drivers/media/dvb/stm/dvb/dvb_v4l2_export.h %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_include_dir}/linux/
cp -dprf linux/drivers/media/dvb/stm/dvb/dvb_cap_export.h %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_include_dir}/linux/
#cp -dprf doxygen/html/* %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_doc_dir}/%{_pkgname}
find doxygen/html/ -name "*" -type f -exec sh -c 'exec cp -- "$@" %{buildroot}/%{_stm_cross_target_dir}/%{_stm_target_doc_dir}/%{_pkgname}' inline {} +

%endif

%clean
rm -rf %{buildroot}

%ifarch noarch
%post -n %{_pkgname}
# Since it is not an error to parallel install player2, we don't want the
# link to the 'latest' to be part of the spec file.
rm -f %{_stm_sources_dir}/player2/player2
ln -s player2-%{version}-%{release} %{_stm_sources_dir}/player2/player2

%preun -n %{_pkgname}
if [ x`readlink %{_stm_sources_dir}/player2/player2` == xplayer2-%{version}-%{release} ] ; then
	rm -f %{_stm_sources_dir}/player2/player2
fi
%endif

%ifarch noarch

%files -n %{_pkgname}
%defattr(-,root,root)
%{_stm_sources_dir}/player2/player2-%{version}-%{release}

%else

%files -n %{_pkgname}
%defattr(-,root,root)

%files -n %{_pkgname}-devel
%defattr(-,root,root)
%{_stm_cross_target_dir}/%{_stm_target_include_dir}/*
%{_stm_cross_target_dir}/%{_stm_target_doc_dir}/*

%endif

%changelog
* Fri Sep 03 2010 Andy Gardner <andrew.gardner@st.com> - int191-1
- Bug 9818 : Remove all linux aware code from Player Wrapper

* Thu Aug 26 2010 Andy Gardner <andrew.gardner@st.com> - int190-1
- Bug 9364 : input crop fix | monitor existing native time baseline across streams
- Bug 8365 : A/V sync loss after seek in a long timeshift
- Bug 7302 : Corrupted video and graphics not being rendered
- Bug 9547 : macroblock structure management | audio collation ignores PTS from an invalid pes header | H.264 reference frame list handling
- Bug 9560 : Crash with SFR-VOD streams
- Bug 9631 : Player2 crash when exiting reverse mode in H264 (Reference frame list handling error)
- Bug 9691 : stmfb 0032 doesn't deinterlace videos properly
- Bug 9611 : re-order the cleanup code after a stream | new PolicyH264TreatTopBottomPictureStructAsInterlaced
- Bug 9718 : AVRD hangs during input format change. Could be caused by silence playing Bug 9680:
- Bug 9683 : Warnings from audio driver about firmware state are obsolete, should be removed
- Bug 9853 : Need to protect firmware from competition for PCM/SPDIF players
- Bug 9871 : Disabling MANIFESTOR_TRACE() violates the rule of least surprise
- Bug 9680 : Havana Kernel 125 Bug in sound/stm/conv.c

* Thu Jun 17 2010 Chris Tomlinson <christopher.tomlinson@st.com> - int189-1
- Some damaged DVB stream crashes havana (bug 6522)
- DivX HD - incorrect display during panning movement (bug 6712)
- Unaligned kernel access on behalf of "Interocitor 0" ... (bug 8315)
- Kernel: Oops: ZN24OutputCoordinator_Base_c18SynchronizeStreams... (bug 8316)
- Too many percussive adjustments (both video and audio)... (bug 8364)
- How can we avoid 'MarkStreamUnPlayable' in case ... (bug 8455)
- Player2 crashes on certain damaged TS  (bug 8784)
- Test broadcast robustness streams (bug 8789)
- WMV / WMA : A/V sync issue after a x2 trick mode (bug 8839)
- Demo clip player bursts audio when exited in paused state (bug 8967)
- Jerky playback of certain TS stream (bug 9009)  
- Adjust v4l scaling crops when playing compressed video file is paused (bug 9055)
- Fixed spurious error when marker frame is received by base codec class (bug 9061)
- Crash EAC3 in TS stream (bug 9080)
- Bottom 5 lines of h264 video are duplicates (bug 9149)
- Severe corruption during v4l zoom when playing compressed video file (bug 9200)
- Fixed bad assignment of audio PTSs to frames (bug 9330)
- DeduceFrameRateFromPresentationTime takes into account frames with spurious timing. (bug 9341)
- Rework manifestor video prints on startup & remove some compile warnings. (bug 9367)

* Wed May 12 2010 Gavin Newman <gavin.newman@st.com> - int188-1
- capture doesn't handle source x/y offsets (bug 8894)
- Enable Fatpipe meta-data mask for word14 (bug 8799)
- Can we detect audio input energy level from the source (bug 8582)

* Mon Apr 12 2010 Gavin Newman <gavin.newman@st.com> - int187-1
- [Dolby certification] jerky sound on EAC3 -> AC3 downmix via SPDIF with player2 INT_179 (bug 8561)
- Add support for raw input from user space (bug 8401)
- v4l2 capture code never sets correct colourspace (bug 8404)

* Wed Mar 31 2010 Gavin Newman <gavin.newman@st.com> - int186-1
- [Dolby certification] Crashing while playing bd_channel_ID_6ch_Lh_Rh_96k.mlp (bug 8527)
- Track selection for dual mono streams is not honored on downmixed outputs (bug 8536)
- Scaling software bug (bug 8566)
- need to integrate kernel blitter into player2 (bug 8574)
- need to be able to convert YCbCr (video range) to RGB (full range) using BDisp (bug 8592)
- [DTS Certification] Stream Type 1-5 incorrectly reported as DTS-HD (bug 8533)

* Wed Mar 24 2010 Gavin Newman <gavin.newman@st.com> - int185-1
- LFE Channel must not be downmixed when in RF_MODE (bug 8137)
- v4l2 capture code never sets correct colourspace (bug 8404)
- improve responsiveness of v4l2 blitting thread (bug 8575)
- [DTS Certification] Stream Type 1-5 incorrectly reported as DTS-HD (bug 8533)
- pcm garbaged sound - dlna certification  (bug 8198)

* Mon Feb 22 2010 Gavin Newman <gavin.newman@st.com> - int184-1
- Mark all files as GPL (bug 8304)
- Allow all player2 modules to be compiled together. (bug 8341)
- Remove reports when printk's disabled (bug 8344)

* Wed Feb 10 2010 Gavin Newman <gavin.newman@st.com> - int183-1
- Cache flushing refactor to work with L2 Cache (bug 7735)

* Wed Feb 03 2010 Gavin Newman <gavin.newman@st.com> - int182-1
- dynamically change screen mode (refresh rate and scanmode) (bug 7805)
- Nicks re-factoring/closedown? branch .... Ringworld (bug 7875)
- Allow smooth resize (bug 7943)
- Nicks closedown branch - The Mothball Prophecy (bug 8010)
- Nicks closedown branch - Revenge Of The Mothball (bug 8021)
- Nicks closedown branch - The Mothball Strikes Back (bug 8096)
- Integrate downmix improvements for DD+ certification (bug 8147)

* Thu Jan 14 2010 Gavin Newman <gavin.newman@st.com> - int181-1
- Nicks re-factoring/closedown? branch .... The Martian Chronicles (bug 7401)
- Add clock recovery ioctl to linux dvb VIDEO_SET_CAMTIM/AUDIO_SET_CAMTIM (bug 7598)
- Nicks re-factoring/closedown? branch .... Janet and John: Off to Play (bug 7788)
- Nicks re-factoring branch .... Fahrenheit 451 (bug 6790)

* Mon Nov 30 2009 Gavin Newman <gavin.newman@st.com> - int180-1
- Nicks re-factoring branch .... Fahrenheit 451 (bug 6790)
- Implement AVS HD player classes (bug 7312)

* Mon Nov 16 2009 Gavin Newman <gavin.newman@st.com> - int179-1
- Downmix of a 2/3 stream to 3/3 stream causes wrong Fatpipe stream type on output (bug 7452)

* Tue Nov 03 2009 Gavin Newman <gavin.newman@st.com> - int178-1
- Nicks re-factoring branch .... Lord Of Light (bug 7299)
- Nicks re-factoring branch .... The road to Wigan Pier (fmi contact : 01454 462666 ) (bug 7326)
- Playing PCM samples using snd_pcm_ causes a 3/4 stream to be sent on Fatpipe (instead of a 2/0 stream) (bug 7373)

* Tue Oct 27 2009 Gavin Newman <gavin.newman@st.com> - int177-1
- Downmix ROM coefficients are not being applied correctly (bug 7319)
- Divx : audio playback stuttering in E19 and E20 (bug 7338)

* Thu Oct 22 2009 Gavin Newman <gavin.newman@st.com> - int176-1
- havana crashes on WMA-V2 streams (bug 4764)
- Nicks re-factoring branch .... Stormbringer (bug 7190)
- CONFIG_DUAL_DISPLAY is on by default (again) (bug 7296)
- DivX HT3 certification fixes & frame parser refactoring (bug 6800)
- Starting the 7108 slog (bug 6373)
- [st3fx] video is jerky when used as a texture (bug 6803)

* Thu Oct 15 2009 Gavin Newman <gavin.newman@st.com> - int175-1
- Def-R/V SPDIF or HDMI input to HDMI output not bit-accurate (bug 6339)
- ERROR in TranslateAudioModeToChannelAssignment: Cannot find matching audio mode (96) (bug 7162)

* Mon Oct 05 2009 Gavin Newman <gavin.newman@st.com> - int174-1
- Nicks hacking branch .... Faster than a speeding bullet (bug 6989)
- LPCM Collator: Sub-class reported absurd frame length (P2-INT-173) (bug 7173)
- Update video codec classes to extract error codes stored in AdditionalInfo_p in MME callback structure (bug 7175)

* Tue Sep 29 2009 Gavin Newman <gavin.newman@st.com> - int173-1
- DTS audio doesn't play on Spiderman stream (bug 7079)
- PES collator: Out of bounds read when processing DTS (bug 3571)
- Protect against invalid LPCM headers (bug 7057)
- input_format provides no distinction between DTS-HD HR and DTS-HD MA (bug 7011)
- Include sysfs control to indicate game mode capability (REQ_0.04_41) (bug 5372)
- Changes for next video firmware release (bug 7172)

* Wed Sep 09 2009 Gavin Newman <gavin.newman@st.com> - int172-1
- Player2 and build config changes required for stmfb -0027 *Patch* (bug 6972)
- Hardware watchpoint support in osinline should be protected by ifdef in INT171 (bug 7026)
- 8-channel 96k dts-master audio is being reported incorrectly (bug 6830)

* Wed Sep 02 2009 Gavin Newman <gavin.newman@st.com> - int171-1
- blank/unblank fails because the video stream inexplicably becomes an audio stream (bug 6702)
- Remove tkdma from player2 (and Bluray Support) (bug 7006)
- PLAYER2_INT_170 Compilation Failure (bug 7017)

* Mon Sep 01 2009 Gavin Newman <gavin.newman@st.com> - int170-1
- Implement mjpeg player classes & hw breakpoint support (bug 6819)
- Support pcm audio (bug 6916)

* Tue Aug 18 2009 Gavin Newman <gavin.newman@st.com> - int169-1
- Merge error of previous release, so re-integrate :
  Post-mortem update for multiple co-processors (bug 6821)

* Mon Aug 17 2009 Gavin Newman <gavin.newman@st.com> - int168-1
- Post-mortem update for multiple co-processors (bug 6821)

* Thu Jul 23 2009 Gavin Newman <gavin.newman@st.com> - int167-1
- Master Playback Latency control does not increase audio latency as expected (bug 6700)
- Really implement AVS video (bug 6719)
- Performance modification for H264 Pre Processor - update to cache control code. (bug 6792)
- When turning off Audio Capture, /sys/class/player2/notify does not get updated (bug 6743)

* Thu Jul 16 2009 Gavin Newman <gavin.newman@st.com> - int166-1
- Correct manifestor done callback to cope with nuances of 
  display driver flushing (bug 6628)
- Bodge to allow BOLT to play (bug 6668)
- Migrate support for cb104 features to player (from 'modifications') (bug 6707)
- Small memory leak when enabling/disabling video capture (bug 6178)
- Can't play AC3 ES stream using dvbtest (bug 6655)
- Disclose timings during AVR shutdown timeouts (bug 6723)

* Thu Jul 09 2009 Gavin Newman <gavin.newman@st.com> - int165-1
- Message "Refusing to reuse SetGlobalCommand structure" appears after 
  successive changes to gain control and AVRD becomes unresponsive (bug 6660)
- Grey Screen of Death - instrumentation (bug 6580)
- How do I select the non-linear scaling mode from V4L? (bug 6181)
- PIP Broken (bug 6629)

* Mon Jul 06 2009 Gavin Newman <gavin.newman@st.com> - int164-1
- Deliver driver fixes to avoid firmware crash when command structures are reused (bug 6577)
- Find out why Vorbis deadlocks after a few plays (bug 6541)
- Integrate AVRD r4892/r5360 (bug 6326)
- Compositor capture - the 3rd coming (bug 6310)

* Mon Jun 29 2009 Gavin Newman <gavin.newman@st.com> - int163-1
- [st3fx] Video texturing (bug 6434)
- Implement Ogg Vorbis player audio classes (bug 5837)

* Wed Jun 24 2009 Gavin Newman <gavin.newman@st.com> - int162-1
- havana hangs on damaged dvb streams (bug 6465)
- Post-mortem code will not compile against aged audio firmware headers (bug 6533)
- snd_ctl_elem_read returns -2 sometimes (bug 6477)

* Fri Jun 19 2009 Gavin Newman <gavin.newman@st.com> - int161-1
- Resolve link issue from release 161 (bug 6489)

* Thu Jun 18 2009 Gavin Newman <gavin.newman@st.com> - int160-1
- Automated firmware post-mortem (bug 6363)
- Video debug infrastructure + bug fixes (bug 6443)
- Error message: index out of bounds coming from linuxdvb interface (bug 6461)

* Mon Jun 15 2009 Gavin Newman <gavin.newman@st.com> - int159-1
- several minor changes, including checking of buffer addresses, disabling first 
  frame early presentation for dvp capture etc... (bug 6303)
- Drop VBI patches, no longer needed (bug 6438)

* Wed Jun 10 2009 Gavin Newman <gavin.newman@st.com> - int158-1
- AVR capture pixel aspect ratio must be communicated through the player (bug 5485)
- Dynamic range compression gain must be off for all audio decoders (bug 6281)
- SPDIF channel status bits for Fatpipe (bug 6229)

* Mon Jun 01 2009 Gavin Newman <gavin.newman@st.com> - int157-1
- E-AC-3 lost sound after seek (bug 6315)
- Compositor capture - stage 2 (bug 6262)

* Mon May 18 2009 Gavin Newman <gavin.newman@st.com> - int156-1
- Put clock recovery functions into place in tree (bug 6222)
- Fix problems comparing current/target rectangles in AVR code (bug 6263)
- Fix stray execute permissions (bug 6268)
- Remove obsolete stm_register_vsync_callback (bug 6282)
- Implement per stream play intervals in LinuxDVB (bug 6265)

* Wed May 13 2009 Gavin Newman <gavin.newman@st.com> - int155-1
- soft muting of individual audio outputs are not working (bug 6206)
- Amixer Master Playback Latency has no effect on the system (bug 6186)

* Mon May 11 2009 Gavin Newman <gavin.newman@st.com> - int154-1
- Linux DVB support for switch stream (bug 6106)
- Modified drain timing to allow drain to continue if frames are coming off display (bug 6191)

* Tue May 05 2009 Gavin Newman <gavin.newman@st.com> - int153-1
- Nicks re-factoring branch .... Pushing Ice (bug 6135)
- Add conditional compile to allow player to be build against BL25 and BL28 audio firmwares (bug 6168)

* Thu Apr 30 2009 Gavin Newman <gavin.newman@st.com> - int152-1
- STD500N - AC3 PIP cause hand when played back (bug 6140)
- Add support for cb104 board, and tidy-up (bug 6161)

* Mon Apr 27 2009 Gavin Newman <gavin.newman@st.com> - int151-1
- TASK ID 14 - Speaker Delay Control (bug 5900)
- Minor changes to pp (bug 6118)
- Fix branch 6099 merge error (bug 6130)

* Wed Apr 22 2009 Gavin Newman <gavin.newman@st.com> - int150-1
- Nicks re-factoring branch: First implementation of the seamless stream switching function. (bug 6019)
- platform.ko remaps DVP pins on all 7200 platforms instead of being platform specific (bug 6099)
- Nicks re-factoring branch .... The Feynman Lectures on Physics (bug 6105)
- Refactor player2 to support Compositor capture pipeline (alpha release) (bug 5988)
- [XH]: Fast backword playback lost lots of frames (bug 5916)

* Wed Apr 15 2009 Gavin Newman <gavin.newman@st.com> - int149-1
- sysfs reports DD+ as "UNKNOWN" (bug 6044)
- Uninitialized variable used as array index when DECIMATED_OUTPUT policy is set (bug 6045)
- FatPipe not working with interactive audio (or any other player2 output) (bug 6077)

* Mon Apr 06 2009 Gavin Newman <gavin.newman@st.com> - int148-1
- Scaling down to a height of less than half of the input window causes output image to be clipped c#10 (bug 5868)
- Nicks re-factoring branch .... The Fountains Of Paradise (bug 5987)

* Wed Apr 01 2009 Gavin Newman <gavin.newman@st.com> - int147-1
- Scaling down to a height of less than half of the input window causes output image to be clipped (bug 5868)
- Event logging, allow any register as the clock source, and platformise the driver (bug 5990)
- FatPipe compensatory latency turned off to avoid firmware bug (bug 5999)

* Tue Mar 31 2009 Gavin Newman <gavin.newman@st.com> - int146-1
- Nicks re-factoring branch .... Last and First Men (bug 5968)

* Mon Mar 30 2009 Gavin Newman <gavin.newman@st.com> - int145-1
- [XH]: MPEG2 I-Frame shows overlapped (5382)
- Nicks re-factoring branch .... The End Of All Songs (bug 5952)
- sysfs reports no change during capture off/on cycle (unless there is a change) (bug 5955)

* Thu Mar 26 2009 Gavin Newman <gavin.newman@st.com> - int144-1
- Develop Theora Player classes (bug 5836)
- Integrate floating Vsync with Audio-LL (bug 5713)
- Add support for 240p and 288p formats to DVP capture (bug 5896)
- Nicks re-factoring branch .... All Fools' Day (bug 5921)
- havana unusable after trying to allocate dvb eit filter (bug 4697)

* Mon Mar 23 2009 Gavin Newman <gavin.newman@st.com> - int143-1
- A new audio driver has to be implemented (bug 5552)
- ModeParamsTable needs to be removed from dvb_avr_video.c (bug 5584)
- Nicks re-factoring branch .... A World Out Of Time (bug 5832)
- Nicks re-factoring branch .... The Kraken Awakes (bug 5901)
- Nicks re-factoring branch .... Creatures of Light and Darkness (bug 5909)
- v4l2 control for pixel aspect correction player policy required (bug 5484)

* Thu Mar 12 2009 Gavin Newman <gavin.newman@st.com> - int142-1
- Integrate Audio-LL V4L2 headers into mainline release (bug 5857)

* Mon Mar 09 2009 Gavin Newman <gavin.newman@st.com> - int141-1
- [XH]: VIDEO_FLUSH sometimes will not return for the certain case. (bug 5657)
- [LL]Need extend the AACSInjectData()'s parameter for AU non-aligned data. (bug 5737)
- Nicks re-factoring branch .... The Shape Of Things To Come (bug 5807)
- [LL]To enhance the playback accuracy of BD player (bug 5760)

* Wed Mar 04 2009 Gavin Newman <gavin.newman@st.com> - int140-1
- 5 channel stereo (Task ID 12) (bug 5522)
- Nicks working branch .... The Day The Earth Caught Fire (bug 5769)
- please integrate 5142 (bug 5782)
- V4L Capture (bug 5783)
- Nicks re-factoring branch .... Invasion Of The Body Snatchers (bug 5785)
- Nicks re-factoring branch .... The War Of The Worlds (original version) (bug 5798)

* Wed Feb 25 2009 Gavin Newman <gavin.newman@st.com> - int139-1
- single step when a buffer is discarded will now continue to step until a buffer is manifested (bug 5678)
- Precursor to switch stream, need to create/manage coded frame buffer in the player NOT codec (bug 5724)
- Video test fails - VC1_AP_L2_014 (bug 5729)
- dvbplay.c using a deprecated define for V4L2 BLANK control (bug 5744)

* Wed Feb 18 2009 Gavin Newman <gavin.newman@st.com> - int138-1
- problem with the display of the 1080 modes (bug 5697)
- Implementation of the NON-vsync locking mechanism for input format switching in dvp capture. (bug 5550)
- Added new version of container parameters header to the h264 frame parser (bug 5630)
- WMV Collator problems in INT_131 (bug 5591)

* Wed Feb 11 2009 Gavin Newman <gavin.newman@st.com> - int137-1
- Add support for all new PTI code (bug 5191)
- Became fix for 5585 (bug 5628)

* Mon Feb 09 2009 Gavin Newman <gavin.newman@st.com> - int136-1
- it's not possible to use V4L2 controls on anything else than input drivers (bug 5619)
- Update player2 to work with latest Real Video codec API (v1.2) (bug 5611)

* Wed Feb 04 2009 Gavin Newman <gavin.newman@st.com> - int135-1
- Overlay Interface On/Off to be reviewed (bug 5532)
- Fixes bug 5128 (bug 5533)
- Automatically adapt between BL025 and BL028 based firmwares (bug 5589)
- Reprocessing elementary stream during error handling interferes with DVD PES private data area handling. (bug 5594)
- add deinterlacing controls to V4L2 (bug 5590)
- Bugzilla_regression - Bug 3929 doesn't play at all  (bug 5569)

* Wed Jan 28 2009 Gavin Newman <gavin.newman@st.com> - int134-1
- Print reductions to improve dvp performace (bug 5527)

* Mon Jan 26 2009 Gavin Newman <gavin.newman@st.com> - int133-1
- Updates to dvp software  (bug 5483)
- Definitions in dvb_avr_video.h need to be available in target filesystem  (bug 5503)

* Wed Jan 21 2009 Gavin Newman <gavin.newman@st.com> - int132-1
- V4L2_CID_STM_BLANK should take immediate effect (bug 4997)
- Implement decimation in dvp input. (bug 5280)
- Decimated decode - Codec support (work item 102) (bug 5413)

* Wed Jan 14 2009 Gavin Newman <gavin.newman@st.com> - int131-1
- SetWindow does't work any longer (bug 5455)
- P2-INT-130 Causes Divx Test Suite to fail (bug 5456)
- MANIFESTOR_TRACE macro has been commented out (bug 5458)

* Mon Jan 12 2009 Gavin Newman <gavin.newman@st.com> - int130-1
- Need to start/stop audio and video capture separately in AVR (bug 3984)
- [HL] INT115: MPEG2 PS audio abnormal after fast foward back to normal play (bug 5316)
- Implement Real Media Video classes (bug 4818)
- Reduce the quantity of data discarded when the audio collators searching for sync words (bug 5432)
- Reduce number of warnings when incomplete downmix firmwares are supplied. (bug 5433)

* Wed Jan 07 2009 Gavin Newman <gavin.newman@st.com> - int129-1
- [HL]M5.1: Panscan display mode works incorrectly (bug 2367)
- WMA audio desynchronization after seek (bug 4708)
- Constant stream of decode errors reported playing back BD-ROM DD+ (bug 5325)
- BLANK CONTROL to be added to V4L2 interface (bug 5334)
- DTS-LBR is not playbacked (bug 5351)

* Thu Dec 18 2008 Chris Tomlinson <christopher.tomlinson@st.com> - int128-1
- Run 7105 H264 preprocessor at better clock speed. (bug 5366)

* Tue Dec 16 2008 Gavin Newman <gavin.newman@st.com> - int127-1
- [LL]Is there any interface from which navigator can know that the linuxDVB has insufficient data during single frame mode. (bug 4909)
- [ZF]: Get the invalid PTS after stopping and replaying the AV device (bug 5200)
- Bypassing HD audio through HDMI (bug 4606)
- Crash when trying to shut down capture with audio only (bug 5342)

* Mon Dec 15 2008 Chris Tomlinson <christopher.tomlinson@st.com> - int126-1
- Updated DivX HD codec API (bug 5332)

* Thu Dec 11 2008 Gavin Newman <gavin.newman@st.com> - int125-1
- Adapt DTSHD codec proxy to new firmware API (bug 5204)
- FLV desyncronization after a seek (bug 5042)

* Mon Dec 08 2008 Gavin Newman <gavin.newman@st.com> - int124-1
- DTS playback in PES stream is broken (bug 5281)
- SYSFS mutex policy to be reviewed (bug 5282)

* Mon Dec 01 2008 Gavin Newman <gavin.newman@st.com> - int123-1
- [XH] M11: devreplay plays one special MPEG2 stream incorrectly (bug 5097)
- [LL]sometimes the video won't display on the screen, but audio can be heard. (bug 5153)
- Addition of ancillary data capture through v4l2 interface for dvp  (bug 5114)

* Thu Nov 27 2008 Gavin Newman <gavin.newman@st.com> - int122-1
- [JP]sometimes writing dts data into /dev/dvb/adapter0/audio0 results in linuxDVB hang when playback DTSCD (bug 4730)
- Does demuxer parse and process audio mixing meta data(spec 8.10.5.3)? Are there any corresponding interfaces available? (bug 5024)
- Bug to collect mixing metadata in the audio codec (bug 5188)
- Inconsistant application of PCM post-processing by decoder driver. (bug 5202)
- Integration of patch to remove obsolete V4L2 controls...  (bug 5215)

* Mon Nov 24 2008 Gavin Newman <gavin.newman@st.com> - int121-1
- Spurious copy of dvbtest has sneaked its way into Player2 (bug 2804)
- re-integrate bug 4468 (bug 5176)
- Addition of HE-AAC LOAS encapsulation parsing (bug 2701)
- DP1: Audio cature at >96KHz (bug 4804)

* Tue Nov 18 2008 Gavin Newman <gavin.newman@st.com> - int120-1
- Removal suspect defines (bug 5114)

* Mon Nov 17 2008 Gavin Newman <gavin.newman@st.com> - int119-1
- More changes to LUT8 (bug 5105)
- The Motion Picture (bug 5083)

* Thu Nov 13 2008 Gavin Newman <gavin.newman@st.com> - int118-1
- First working attempt of the controls for AVR video capture.  (bug 5083)

* Mon Nov 10 2008 Gavin Newman <gavin.newman@st.com> - int117-1
- Add tuner and pti code to 7105 platform driver (bug 4811)
- Capture On/Off crashes after some time (bug 5030)
- Changes to video encoding in stm_ioctls.h breaks binary compatability (bug 5065)

* Thu Nov 06 2008 Gavin Newman <gavin.newman@st.com> - int116-1
- Player2 DivX HD Support (bug 4489)
- Implement "Number of frames captured" (bug 4512)
- sysfs input_format file disappears when capture is disabled - that's really inconvenient! (bug 4869)
- Add mb442 tuner support (bug 4983)
- Question: video buffer capture ? (bug 4990)
- i2c placeholder (bug 5047)

* Mon Nov 03 2008 Gavin Newman <gavin.newman@st.com> - int115-1
- [YM] How to setup V4L2 in LUT8 mode (bug 3940)
- [LL]Issues of Single frame backward (bug 4860)
- PTI modification to allow complete mux to be dumped. (bug 4982)

* Thu Oct 16 2008 Gavin Newman <gavin.newman@st.com> - int114-1
- Volume controls not working correctly (channel 7 influences channels 0 and 6 as well) (bug 4677)
- work on colour matrices (bug 4841)
- Playback of LPCM from DVD-audio discs is broken (PLAYER2_INT_112) (bug 4855)
- New version of fixes for 4603, needs to be 'out there' (bug 4883)

* Mon Oct 06 2008 Gavin Newman <gavin.newman@st.com> - int113-1
- Player does a stack dump when attempting to play some divx streams (bug 4821)
- MPEG4 part 2 ts could not be played (bug 4442)
- AVR code is horrific to maintain (bug 4802)
- Allow use of master mapping for DTS handling when content is to be discarded before decode (bug 4814)

* Thu Oct 02 2008 Gavin Newman <gavin.newman@st.com> - int112-1
- Fix V4L issue with clut physical buffers (bug 4773)
- further tweaks of the output rate adjustment (bug 4786)
- B10: Capture on/off regression introduced by faster audio shutdown changes (bug 4793)
- Correct list of supported aspect ratio's for H264 (bug 4795)
- B10: Audio capture at >48KHz (bug 4805)
- Disable post-decode downmix and DC removal (bug 4812)

* Mon Sep 29 2008 Gavin Newman <gavin.newman@st.com> - int111-1
- WMA volume modification introduce glitches and seeks (bug 4719)
- Doxygen documents wrong class (bug 4662)
- fiddles with output rate adjustment (bug 4765)
- Remove queue failure handling in manifestor (bug 4768)
- Bits and pieces of an old implementation of Audio Emergency Mute (bug 4769)

* Tue Sep 23 2008 Gavin Newman <gavin.newman@st.com> - int110-1
- Modify V4l to support 656 control & pixel offset (bug 4510)
- Display driver mode tableto be updated (bug 4687)
- Continuous improvement of DVP video capture (bug 4745)

* Tue Sep 22 2008 Gavin Newman <gavin.newman@st.com> - int109-1
- CONFIG_DUAL_DISPLAY is incorrectly disabled in PLAYER2_INT_108 (bug 4750)
- Timeout on capture error is way too long (bug 4751)
- Satisfy minor sysfs feature requests for AVR  (bug 4752)

* Mon Sep 22 2008 Gavin Newman <gavin.newman@st.com> - int108-1
- Moving on, down the line (bug4632)
- Mixer starvation when 32KHz audio is reproduced with 
  330ms latency (bug 4739)
- squeezed letter-box on H.264 TS (bug 4672)
- How do I crop the input video buffer? (bug 4648)

* Thu Sep 11 2008 Gavin Newman <gavin.newman@st.com> - int107-1
- dvp vsync locking, LDVB_VID / AUD - Player crash, 
  H264_MAIN_SYNTAX crash (bug 4657)
- Volume controls not working correctly (channel 7 influences 
  channels 0 and 6 as well) (bug 4677)

* Mon Sep 08 2008 Gavin Newman <gavin.newman@st.com> - int106-1
- Core of DTS-HD streams is not bypassed via SPDIF (bug 3721)
- Added player policy PolicyLimitInputInjectAhead (bug 4646)

* Thu Sep 04 2008 Gavin Newman <gavin.newman@st.com> - int105-1
- DVB A/V synchro takes too much time to recover after a 
  glitch (bug 4603)
- pre-alpha release of the low latency video work (bug 4115)
- Pixel aspect ratio incorrect if display extension present (bug 4605)
- Correct manifestor queue failure conditions (bug 4612)
- Support WMA 9 pro etc (bug 4631)
- [review] Player audio is corrupted when using BPA region 1 
  on STL2.3 32bit mode (bug 3374)

* Thu Aug 28 2008 Gavin Newman <gavin.newman@st.com> - int104-1
- [Devel] Passthru of AC3/DTS metadata to FatPipe (bug 4043)
- User configurable downmix co-efficients (bug 4521)
- Unhandled DTS frame type  (bug 4601)
- [Enhancement] Transcode DD+ to AC3 (for spdif output) (bug 3668)
- Crop circles/rectangles (bug 4390)

* Thu Aug 21 2008 Gavin Newman <gavin.newman@st.com> - int103-1
- WMA decoding sounds wrong (bug 4462)
- Implement system monitor (bug 4412)
- Provide support for 1080-50p and 1080-60p in capture 
  interface (bug 4505)
- FatPipe channel encoding is broken (again) (bug 4581)

* Tue Aug 19 2008 Gavin Newman <gavin.newman@st.com> - int102-1
- SYSFS - Audio Emergency Mute (bug 4363)
- Fix issue with updated platform driver (bug 4567)

* Mon Aug 18 2008 Gavin Newman <gavin.newman@st.com> - int101-1
- SYSFS architecture needs some tidy up (bug 4116)
- Player2 hardcode the L0/R0 Stereo Downmix while AC3 
  certification requires LtRt downmix (bug 4308)
- Player updates to support latest ST Processors (bug 4418)
- Bad pre-emption during mixer parameter update causes crash (bug 4502)
- Modify V4l to support 656 control (bug 4510)
- Document where the volume limiter's mute/unmute timings are 
  located (bug 4520)

* Mon Aug 11 2008 Gavin Newman <gavin.newman@st.com> - int100-1
- Havana crashes when zapping TV while changing "master" 
  volume (bug 4482)
- BL023_03-1 introduces subtle audio delay on stream startup (bug 4438)

* Thu Aug 07 2008 Gavin Newman <gavin.newman@st.com> - int99-1
- Add support for priority based filterring on transport 
  streams (bug 4456)
- Audio manifestor incorrectly infers the passage of time 
  (playback time more than 5 seconds in the future) (bug 4461)
- [dy] PID filter of IG data in STD100P not working? (bug 4468)
- Parse DTS-HD header to get the correct frame properties (bug 4277)

* Thu Jul 31 2008 Gavin Newman <gavin.newman@st.com> - int98-1
- BL23_3BD API update  (bug 4233)
- Implementation of Sample Rate Detection (bug 4361)
- Implementation of Audio Emergency Mute  (bug 4362)

* Mon Jul 28 2008 Gavin Newman <gavin.newman@st.com> - int97-1
- [LL] Why I still can hear the audio after set AUDIO_SET_MUTE 
  with true. (bug 4263)
- Forward error correction control is not honours by driver (bug 4307)
- Rebased version of bug 4399  (bug 4421)

* Wed Jul 23 2008 Gavin Newman <gavin.newman@st.com> - int96-1
- V4l2 Format Recognizer+AAC Controls (bug 3661)
- Fix hms1.h in platform.c to the NEW SH_ST_HMS1 macro (bug 4253)
- Extended ksound with snd_pcm_htimestamp() (bug 4353)
- Implement Player2 VP6 classes (bug 4147)

* Fri Jul 18 2008 Gavin Newman <gavin.newman@st.com> - int95-1
- Bad mixer state transition if audio is stopped but no samples
  have ever been played (aka Pirate FBI warning bug) (bug 4176)
- Deadlock when playing low resolution streams (bug 4373)

* Tue Jul 08 2008 Gavin Newman <gavin.newman@st.com> - int94-1
- havana crashes when going to speed 2 and issuing AUDIO_PLAY 
  command on DVB stream (bug 4281)
- Real time DTS/AC3 encode for 96KHz and 192KHz streams (bug 4178)
- 114 Kernel : bpa2.h:45: error: field ‘res’ has 
  incomplete type (bug 4295)

* Wed Jul 02 2008 Gavin Newman <gavin.newman@st.com> - int93-1
- alsa volume control disturb dvb playback  (bug 4213)
- Glitch at start of SD streams  (bug 3989)
- Implement Dolby-mandated dynamic range settings for DVB (bug 4207)
- Audio debug causes stream judder - P2_INT_91 (bug 4221)
- [H264] havana hangs when playing a certain h264 
  encoded stream (bug 4239)

* Mon Jun 23 2008 Gavin Newman <gavin.newman@st.com> - int92-1
- CDDA can't playback on st7200 board with M10 (bug 4099)
- Audio and Video capture crash when trying to exit  (bug 4085)
- Player crashes on shutdown while playing 22Khz audio 
  file (with DivX video) (bug 4136)
- Enable 44.1KHz DTS encoding  (bug 4208)

* Wed Jun 18 2008 Mark Einon <mark.einon@st.com> - int91-1
- Enable full-scale HD codecs (bug 3321)
- audio mutes randomly when playing DVB streams (bug 3886)
- [Devel] Optimize mixer communications for >48Khz output (...  (bug 4041)
- [Devel] Deploy real-time AC3 and DTS encoders (over SPDIF) (bug 4042)
- DIVX_VID_23 - Video too quick - P2_INT_90 (bug 4152)
- [H264] Mosaic effect when decoding through the LinuxDVB T... (bug 4162)
- DIVX_VID_23 - No Audio - P2_INT_90  (bug 4169)

* Thu Jun 12 2008 Gavin Newman <gavin.newman@st.com> - int90-1
- [WMA] PTS values not updated after seek  (bug 4106)
- Add Front end support for 7200 to linux drivers  (bug 3869)
- Audio manifestor breaks if GetDecoderBufferPool() is 
  called twice (bug 4133)
- Check for MP3 capability in MPEG codec proxy (bug 4155)
- Implement Player2 H263 classes (bug 4022)

* Thu Jun 05 2008 Gavin Newman <gavin.newman@st.com> - int89-1
- WMA decoding fails and hangs havana  (bug 3584)
- AVR: Automatically recover after channel swap is detected (bug 3782)
- ADDITIONAL SYSFS FEATURES  (bug 4030)
- contains fix for 4023, and first implementation of 
  work for 3777 (bug 4039)
- DivX codec doesn't always take in account the DivX version (bug 4075)
- Audio Capture not working with player-int88-1 (bug 4082)
- typo in pseudo_mixer.c (bug 4087)

* Tue May 27 2008 Gavin Newman <gavin.newman@st.com> - int88-1
- WMA decoding fails and hangs havana (bug 3584)
- h264 reverse - alpha (bug 3986)
- Re-introduce audio PTS validation (bug 4034)
- Bug 3882 was incorrectly updated shortly 
  before merging (bug 4040)
- [Devel] Implement FLV file parsing in dvbtest (bug 3957)

* Thu May 22 2008 Gavin Newman <gavin.newman@st.com> - int87-1
- Fatpipe channel encoding not following Fatpipe 
  specification 1.1 for stereo streams (bug 3675)
- Integrate sysfs with 3837 ( also contains 3942 ) (bug 3882)
- How to set the output window of havana ? (bug 3970)
- 22kHz mp3 file is playbacked too fast (bug 3977)
- LPCM collator emits incomplete frame after 
  an AUDIO_CLEAR_BUFFER (bug 3983)
- Too few samples reserved for de-pop when pre-mix 
  downsampling is deployed. (bug 3997)

* Fri May 16 2008 Gavin Newman <gavin.newman@st.com> - int86-1
- [PLAYER2_INT_85] STLinux22 and 23 Failed last night (bug 4001)

* Thu May 15 2008 Gavin Newman <gavin.newman@st.com> - int85-1
- Attempting to bypass DTS-CDDA causes firmware to deadlock (bug 3660)
- Audio output rate adjustment (fsynth tuning) regressed 
  in STLinux-2.3 (bug 3938)
- ***** Fatal-Unknown Task: Buffer_Generic_c::ObtainMetaDataReference - 
  Attempt to act on a buffer that has a reference count of zero. (bug 3947)
- Fixes 3847, 3866, 3892, 3924, and 3935 (bug 3952)
- [Devel] Distribute the audio processing across both 
  audio co-processors (bug 3972)
- Default STx7200/MB519 topology neglects formatting information 
  on HDMI (bug 3985)
- [Devel] Implement frequency capping at mixer inputs  (bug 3987)
- ALSA underrun issue and remapping using kernel 111 (bug 3994)

* Thu May 8 2008 Gavin Newman <gavin.newman@st.com> - int84-1
- Eac3 vs AC3 firmware capability verification is buggy (bug 3953)
- PLAYER2_INT_83 does not compile on STLinux-2.2 (bug 3953)
- [DIVX] Havana DivX frameparser handle wrongly the 
  PAR_EXTENDED aspect ratio mode (bug 3934)

* Wed May 7 2008 Peter Bennett <peter.bennett@st.com> - int83-1
- 3625 Interactive audio does not work on STLinux-2.3
- 3802 Nicks new dicotyledon
- 3841 Properly identify "CLSID_AsfXSignatureAudioErrorMaskingSt...
- 3898 [Devel] Move audio topology management from debugfs to ALSA
- 3909 Adding partial fixes for : audio mutes randomly when play...
- 3921 Adopt the enumerated values for audio firmware channel or...
- 3929 [AAC] some AAC stream isn't decoded right
- 3926 Codec (Task #1) NULL pointer dereference handling MarkerB...
- 3934 [DIVX] Havana DivX frameparser handle wrongly the PAR_EXT...
- 3949 Nicks new Hypocotyl
- 3951 NULL pointer dereference in Codec_Mme_Audio_c::Input()

* Wed Apr 30 2008 Gavin Newman <gavin.newman@st.com> - int82-1
- Add support for audio channel select (bug 3837)
- Get gprof to work with dvbtest (bug 3674)
- Mixer remains running when MIXER_PAUSE is applied on boundary (bug 3551)
- WMA Fails to Play - P2_INT_81 (bug 3894)

* Mon Apr 28 2008 Gavin Newman <gavin.newman@st.com> - int81-1
- EAC3 codec proxy badly ported to ACF_firmware-BL021_11 (bug 3850)
- strange behaviors on MIXER0 gain control (bug 3883)
- [IEC60598] Add iecset support to the MIXER0 pseudo soundcard (bug 3520)
- Coded data bypass does not work if underlying codec is 
  not available (bug 3848)
- Player updates to support vid firmware pb09 13 (bug 3888)
- Deploy firmware status verification in all audio codecs (bug 3774)
- Limiter is currently enabled in mixer proxy even when 
  coded data bypass is requested (bug 3862)
- Isolated B9 bug fixes to 8 pixel offset (bug 3890)

* Tue Apr 22 2008 Gavin Newman <gavin.newman@st.com> - int80-1
- Fatpipe metadata InvalidStream is not passed (bug 3798)
- Fix 8 pixel offset and allow fast switching (bug 3835)
- Enhancement] Enable AAC SBR decoding (bug 3820)

* Thu Apr 17 2008 Gavin Newman <gavin.newman@st.com> - int79-1
- Wma crash, incorrect return value on write, 
  protection against access when not open (bug 3793)

* Mon Apr 14 2008 Gavin Newman <gavin.newman@st.com> - int78-1
- mp4 range checks, event term, output timer changes (bug 3726)
- Modified manifestor, to avoid returning outrageous restart 
  times when the player has used UNSPECIFIED_TIME as a 
  requested display time. (bug 3764)
- Fix the latency problems caused by post-mix sample rate 
  conversion. (bug 3775)
- [Devel] Implement master A/V latency adjustment (bug 3776)
- changes to correct calculation of restart time after 
  a pause. (bug 3778)
- [Devel] Finalize implementation of *cb101* metadata API (bug 3785)
- Audio manifestor does not support QueueEventSignal() (bug 3796)
- *cb101* regression in PLAYER2_INT_77 (bug 3797)
- Creation d'un postprocessing Gain + Limiter (bug 3628)

* Mon Apr 07 2008 Gavin Newman <gavin.newman@st.com> - int77-1
- different versions of MPEG2_VideoTransformerTypes.h and 
  friends exist twice in the system (bug 3698)
- WMA certification tests for WMAv2 contain an unexpected 
  error correction GUID (bug 3733)
- [Devel] HDMI audio by default (STx7200) (bug 3734)
- Rework event creation/destruction, Restructuring of 
  wma codec (bug 3744)
- [Devel] Deploy automatic resampling of low sampling 
  frequency streams (bug 3753)
- player2 uses virt_to_phys() on ioremap()ed memory (bug 3754)
- Make loading tkdma module optional (bug 3756)
- Fix frontend devices for player2 (bug 3757)

* Tue Apr 02 2008 Gavin Newman <gavin.newman@st.com> - int76-1
- EAC3 parser does not distinguish AC3 from DD+ (bug 3720)
- Player_Generic_c::RecordNonDecodedFrame - Unable to record, 
  table full. message output when shutting down 
  stream (P2_INT_75) (bug 3727)
- Memory leak fixing (bug 3728)
- BUG() macro called when trying to access ALSA control 
  "IEC958 Playback Mask" (bug 3729)
- Bad merge makes SPDIF bypass unusable in PLAYER2_INT_75 (bug 3730)

* Tue Apr 01 2008 Gavin Newman <gavin.newman@st.com> - int75-1
- [IEC61937] Get elementary stream to the firmware! (bug 3593)
- Provide stubbed implementation of the new ALSA controls (bug 3624)
- Remove hard links to 2.3 dirs in player2/Makefile (bug 3638)
- lockup of player when decoding some H264 streams at x6..8, 
  Kernel panic playing some H264 MP4 files, 
  Fixed DVR lost buffer crash on shutdown. (bug 3670)
- Added GET_PLAY_TIME ioctls for video and audio (bug 3681)
- Improve buffer diagnostics for DVP (bug 3684)
- Tweak codec selection policy for DivX vs Mpeg4p2 (bug 3707)
- ALSA amixer controls documentation error (bug 3690)
- [STLinux-2.3] STx710x hardware MPEG decoder not working (bug 3270)
- dvr demux device misdetect normal TS packets as bluray packets (bug 3653)

* Wed Mar 26 2008 Gavin Newman <gavin.newman@st.com> - int74-1
- Modify IC data for field pictures in vc1. Correctly reject 
  422 streams. Correctly reject mpeg4p2 < version 5. (bug 3503)
- Error Playing BBC -HD 264 stream (bug 3676)
- [DIVX] MPEG4P2 stream in TS (bug 3097)
- DVB sections filtering on player2's TS demux 
  (was "DMX_SET_SOURCE ioctl") (bug 3654)

* Tue Mar 25 2008 Peter Bennett <peter.bennett@st.com> - int73-1
- [Devel] Fix DD+ parser to enable transcoding to Ac3 (bug 1776)
- support configuring FlexVP (TNR) from v4l2 (bug 3553)
- Fixed bug where elapsed time since last supplied pts 
  was an int when it should have been a long long int. 
- Effectively commented out the use of the trick mode 
  where we discard all B frames, and decode the P 
  frames in a degraded manner. (bug 3612)
- Add support for 5.1 analogue output (bug 3617)
- player2 version of stmvout.h is out of date (bug 3639)


* Tue Mar 11 2008 Peter Bennett <peter.bennett@st.com> - int72-2
- Updated dvp for stmfb update (bug 3607)
- Pauses in discontinuities (bug 3537, 3585, 3598)

* Tue Mar 11 2008 Peter Bennett <peter.bennett@st.com> - int71-2
- Updated dependencies

* Thu Mar 06 2008 Gavin Newman <gavin.newman@st.com> - int71-1
- Corrected size of input buffer to h264 transformer,  
  Widened jump detection error margin, 
  Added slice order check for baseline profile. (bug 3575)
- Fix issues with v4l bpa allocated buffers (bug 3583)

* Mon Mar 03 2008 Gavin Newman <gavin.newman@st.com> - int70-1
- Percussive adjustments are not subject to de-click and are 
  not accurately applied (bug 3263)
- IceAge.ts DTS Issue (P2_INT_65) (bug 3435 via revision 
  dissaproval from bug 3319)
- MLP codec modifications to support BL21-6 audio firmware (bug 3548)
- Restricted SEI playload to fit in an anti-emulation buffer,add 
  policy to enable the attempted decode of baseline profile h264 (bug 3550)
- Abuse of snd_pcm_set_silence (bug 3563)

* Thu Feb 28 2008 Gavin Newman <gavin.newman@st.com> - int69-1
- Port Player to STLinux2.3 (bug 2889)

* Thu Feb 28 2008 Gavin Newman <gavin.newman@st.com> - int68-1
- kernel oops in VIDIOC_G_INPUT when called w/o VIDIOC_S_INPUT (bug 2738)
- Jump Issues, Smooth Reverse Checking, drain timeout, 
- buffers avoiding 64Mb boundaries (bug 3539)

* Thu Feb 21 2008 Gavin Newman <gavin.newman@st.com> - int67-1
- 16 bytes 8 pixel offset in DVP capture (bug 3376)

* Mon Feb 18 2008 Gavin Newman <gavin.newman@st.com> - int66-1
- residual frames "flickering" on divx with mpeg4p2 firmware (bug 3372)
- Stream crashes: lol-xine.aac and lol-vlc.aac (bug 3449)
- [V4L] YUV plane doesn't seem usable anymore in INT_64 (bug 3462)
- H264 reverse play - preliminary work (bug 3365)
- Crash of the player when AVRD is launched with no sound enabled (bug 3428)
- AUDIO_FLUSH returns EFAULT in paused state. (P2_INT_64) (bug 3434)
- AAC collator does not verify frame lock properly (bug 3464)
- [SPDIFin] Coded data does not playback cleanly (P2_INT_65) (bug 3471)
- VC1 review / correct ldvb ret values (bug 3441)

* Mon Feb 11 2008 Gavin Newman <gavin.newman@st.com> - int65-1
- Stream hangs: MPEGSolution_stuart.mp4 (bug 3319)
- support configuring IQI from v4l2 (bug 2897)
- Deploy IEC60598 (LPCM) encoding on SPDIF outputs (bug 3356)
- WMA does not start (P2_INT_62) (bug 3358)
- [devel] Add CRC checking to audio and video outputs (bug 3378)
- Evaluate interruptible locks in linuxdvb (bug 3384)
- v4l2_multiplane test kernel NULL ptr deref (P2_INT_64) (bug 3404)

* Mon Feb 04 2008 Gavin Newman <gavin.newman@st.com> - int64-1
- 480p input capture freezes the player (bug 3323)
- Support post processing, TM domain change events,
  check allowed decodes (bug 3331)

* Thu Jan 31 2008 Gavin Newman <gavin.newman@st.com> - int63-1
- Monotonic Timer (bug 3305)
- Trick mode domain change event (bug 3354)

* Wed Jan 30 2008 Gavin Newman <gavin.newman@st.com> - int62-1
- H264 Juddering Playback on the 7109 (bug 3272)
- Player crashes when demux0 is not configured. (bug 3332)
- [V4L2] palette for v4l buffers in CLUT pixel format have
  the first two entries wrong (bug 3111)
- Update Mixer driver wrt FW BL021_3BD API. (bug 3239) 
- linux apm support (3275)
- Issuing AUDIO_CLEAR_BUFFER causes WMA playback to deadlock (bug 3309)
- Added post processing control structures to the system (bug 3312)
- Update DivX codec to mark all errors as stream unplayable (bug 3325)
- Deploy field interpolation when trying to perform pulldown of
  interlaced content on an internlaced display (bug 3344)
- 1080i@50 is offset (bug 3349)
- SD Resolutions don't work (bug 3350)
- Rescaling does not fit to whole screen (bug 3351)

* Fri Jan 25 2008 Gavin Newman <gavin.newman@st.com> - int61-1
- 7109 PIP & ignore pre-processor errors, and to impose a sensibility (bug 3304)
- Issuing AUDIO_STOP/AUDIO_PLAY cause WMA playback to crash (bug 3310)
- dvp_video_close and dvp_audio_close need to be redefined (bug 3320)
- [V4L2] device is exclusive to one process (bug 2973)

* Wed Jan 23 2008 Gavin Newman <gavin.newman@st.com> - int60-1
- [H264] crash: H.264 in AVI (bug 3255)
- MLP collator modifications to support DVD-Audio (bug 3092)
- unplayable and decode failure events, deinterlacing control, 
  divx bug fix, step fix. (bug 3226)
- Audio breaks up playing to MIXER0 (bug 3288)
- Corrected initialization of long term flag on ref list 2. (bug 3297)

* Mon Jan 21 2008 Gavin Newman <gavin.newman@st.com> - int59-1
- AV sync implementation (bug 3169)
- Add Policy to cope with broadcast streams that claim some
  some interlaced frames are progressive (bug 3211)
- Insufficient Memory for PIP (bug 3240)
- Changed rational calculation to improve accuracy. (bug 3242)

* Thu Jan 10 2008 Gavin Newman <gavin.newman@st.com> - int58-1
- reduce h264 coded data buffer (bug 3172)
- ring generic code - prevent extracting previously extract... (bug 3206)

* Mon Jan 07 2008 Gavin Newman <gavin.newman@st.com> - int57-1
- [JY]dvbtest hang after it playback the DTS CD audio data ... (bug 3161)
- Support for MME logging thanks to a dedicated module (bug 2765)
- Act upon relayfs review comments (bug 2839)
- should be h264 reverse play, since I won't be working on ... (bug 3068)
- WMA does not start (P2_INT_55)  (bug 3157)
- PLAYER2_INT_56 spuriously (and incorrectly) enables CONFI... (bug 3163)

* Wed Jan 02 2008 Gavin Newman <gavin.newman@st.com> - int56-1
- JW Development tree (bug 3073)
- Pete's Shanghai wavefront (bug 3098)
- Engineering B8 (with memory leak patch) hangs after ~6.. (bug 2911)

* Fri Dec 14 2007 Gavin Newman <gavin.newman@st.com> - int55-1
- Nicks new working tree i.e. h264 pan scan (bug 3039)
- Bugzilla issues - including this one (bug 3040)
- Support of HD-DVD pes formatting (MLP-Dolby TrueHD)
- Implement AUDIO_GET_PTS (bug 3059)
- Divx files which contain NVOP frame fail to play (bug 3063)
- Support dropped frmaes from DivX AVI files (bug 3064)

* Tue Dec 10 2007 Gavin Newman <gavin.newman@st.com> - int54-1
- BD playback stalls with trace output - Big Fish (bug 2892)
- Fix VC1 Intensity Compensation (bug 2946)
- Your guess is as good as mine, possiblly reverse h264, bu... (bug 3020)

* Mon Dec 09 2007 Gavin Newman <gavin.newman@st.com> - int53-1
- An as yet unspecified enhancement - nicks working tree (bug 2999)
- Correct platform information to include hms1 specific pti... (bug 3006)
- Make use of dynamic video buffering in AVR capture (bug 3007)
- If the resolution is too large to playback using current ... (bug 3019)
- DTShd audio Collator failure (bug 3022)

* Thu Dec 05 2007 Gavin Newman <gavin.newman@st.com> - int52-1
- ioctl(v4l2Handle,VIDIOC_S_INPUT,&input) return -1 and... (bug 2794)
- Investigate and move forward on dynamic decode buffer siz... (bug 2901)
- Erroneous sampling frequencies look up table (bug 2965)
- AVR Video capture fails on INT51 (bug 2981)

* Wed Nov 28 2007 Peter Bennettn <peter.bennett@st.com> - int51-2
- Modified package to include dependencies for other pacakages
- PLEASE REMEMBER TO UPDATE THE REQUIRES LINE NEAR THE TOP OF THE FILE

* Wed Nov 28 2007 Gavin Newman <gavin.newman@st.com> - int51-1
- add new player internal api allow to recognition and hand.. (bug 2884)
- Support VC1 Simple and Main profiles (bug 2885)
- Modify Player such that we can multiple pre-processors (bug 2887)

* Wed Nov 21 2007 Gavin Newman <gavin.newman@st.com> - int50-1
- [Devel] Support for TrueHD audio codec (bug 2522)
- Extend buffer class and Codec to allow allocation from sp... (bug 2824)
- Playback of DivX files 2003 part.1.avi 2003 part.2.avi fails (bug 2844)
- Correct the 'sense' of the Master Volume Bypass switch (bug 2849)
- H264 macroblocks on BBC stream after Skip (bug 2864)
- Nicks working tree (bug 2865)
- add extra relayfs taps, to collator (bug 2873)

* Thu Nov 15 2007 Gavin Newman <gavin.newman@st.com> - int49-1
- AVR audio crashes on second run of VideoTest (P2_INT_46) (bug 2778)
- [Devel] DTS-HD LBR support (bug 2432)
- New audio firmware wk43 2007 (bug 2624)
- Added new ioctls to DVP capture interface (bug 2797)
- Require RelayFS interface (bug 2635)

* Thu Nov 13 2007 Gavin Newman <gavin.newman@st.com> - int48-1
- ksound-core.c has been badly merged into the player sources (bug 2777)
- Ongoing development BPA2 (bug 2780)
- Improve time reporting after deploying percussive adj (bug 2803)

* Thu Nov 08 2007 Gavin Newman <gavin.newman@st.com> - int47-1
- Continue debugging of H264 - moving to high profile synta... (bug 2756)
- Provide audio Picture-and-Picture support (bug 2758)
- 480i capture color inversion (bug 2761)

* Thu Nov 08 2007 Gavin Newman <gavin.newman@st.com> - int46-1
- Enhance LPCM playback with fixed-size frames (bug 2759)
- Audio Capture Fix (bug 2764)

* Wed Nov 07 2007 Gavin Newman <gavin.newman@st.com> - int45-1
- Performance improvement for 0D copy (bug 2415)
- Continuing development - look at some more allegr... (bug 2595)
- Audio Capture (bug 2665)
- WMA regression (P2_INT_43) (bug 2694)
- Dual Video Display (bug 2719)

* Thu Nov 01 2007 Gavin Newman <gavin.newman@st.com> - int44-1
- VLPCM collator generates the bad timestamps for some.. (bug 2572)
- Support for out of mux in playback streams (bug 2628)

* Mon Oct 29 2007 Gavin Newman <gavin.newman@st.com> - int43-1
- Implement SPDIFin decoder driver (bug 2033)
- WMA crashes on kernel with prempt turned on (bug 2630)

* Tue Oct 23 2007 Gavin Newman <gavin.newman@st.com> - int42-1
- DTS collator fails to comprehend DVD-style encapsulation (bug 2567)
- Mixer cannot be restarted on Stx7200 is the primary sound.. (bug 2583)
- lpcm corruption (P2_INT_40) (bug 2599)

* Wed Oct 17 2007 Gavin Newman <gavin.newman@st.com> - int41-1
- M5.1 Panscan display mode works incorrectly (bug 2367)
- Fatpipe encoder does not operate correctly on the output.. (bug 2584)
- DIVX and H264 codecs fail to decode (P2_INT_40) (bug 2600)

* Thu Oct 11 2007 Gavin Newman <gavin.newman@st.com> - int40-1
- Performance improvement for 0D copy (bug 2415)
- Increase mixer configuration options (bug 2536)
- check performce of h264 decode, restore to full.. (bug 2579)
- player2 on X and i386 support (bug 2054)
- Player2 DVP capture port (bug 2551)
- Unclean (and very long) termination of WMA playback (bug 2502)

* Tue Oct 09 2007 Gavin Newman <gavin.newman@st.com> - int39-1
- player2 on X and i386 support (bug 2054)
- Downmix not working in ACC_firmware-BL019_02-4 (bug 2517)
- Player2 DVP capture port (bug 2551)
- VLPCM collator generates the same timestamp for all packets (bug 2571)

* Wed Oct 03 2007 Gavin Newman <gavin.newman@st.com> - int38-1
- Player2 support for 7200 and proper platformisation (bug 2398)
- DVD playback has regressed in PLAYER2_INT_36 (bug 2521)

* Thu Sep 27 2007 Gavin Newman <gavin.newman@st.com> - int37-1
- Failure to play audio and video together in DivX VID-3A.. (bug 2472)
- Fix for latest version of STGFB (stbfb 3.0 stm23 0008) (bug 2507)
- Further work no vc1 reverse (bug 2504)
- STx7200 support in player2 (bug 2510)
- player2 Makefile issue (bug 2508)

* Mon Sep 26 2007 Gavin Newman <gavin.newman@st.com> - int36-1
- DVP Driver Extension (bug 2464)
- Lockup when playng back VID-2B.avi (bug 2487)
- Performance change for Divx MME (bug 2488)
- AVSYNC turned of by default (bug 2489)
- VC1 reverse play (bug 2467)

* Mon Sep 24 2007 Gavin Newman <gavin.newman@st.com> - int35-1
- Further cleaning and polishing (bug 2292)
- kernel oops in stm_v4l2 (bug 2450)
- Add PTI driver to player2 (bug 2053)
- player2 on X and i386 support (bug 2054)
- This time I really am going to go for single step honest honest... (bug 2420)

* Wed Sep 19 2007 Gavin Newman <gavin.newman@st.com> - int34-1
- TrickMode Crash (f->c->x) P2_INT_24 (bug 2108)
- Performance improvment for 0D copy (bug 2415)
- Temporary bug to take developments related to 2395 (bug 2437)
- VC1 regression in P2_INT_32 (bug 2424)

* Mon Sep 17 2007 Gavin Newman <gavin.newman@st.com> - int33-1
- Really single step this time (bug 2393)
- I really am going to look at single step this time (bug 2405)
- MME Callback threads not running at the same priority as ... (bug 2406)
- Audio decode buffers (uncompressed) are *way* too large (bug 2412)
- Implement WMA support in Player2 (bug 2210)

* Wed Sep 12 2007 Daniel Thompson <daniel.thompson@st.com> - int32-1
- Continue debugging of H264 (bug 2301)
- divxtest's H_VIDEO_CODECS/VID-2A.avi regression (bug 2362)
- Continue addressing H264 debug issues (bug 2366)
- Continue work on 264 - single step (bug 2387)
- Get on and implement single step (bug 2392).

* Thu Sep 6 2007 Gavin Newman <gavin.newman@st.com> - int31-1
- 2327 OutputTimer_Base_c::DecodeInTimeFailure - Failed to deliv...  
- 2328 problems with HD MPEG2 on 7100 boards (using MPEG2_HW_MME...  
- 2334 Minor updates to the player2 packaging mechanism 

* Mon Sep 3 2007 Daniel Thompson <daniel.thompson@st.com> - int30-1
- Minor tidy-up of the packaging.

* Mon Jun 4 2007 Peter Bennett <peter.bennett@st.com> - int22-1
- Updated to new build system.
