Summary:	STM Frame Buffer driver source
Name:		%{_stm_pkg_prefix}-stmfb
Version:	3.1_stm23_0032
Release:	1
URL:		http://www.stlinux.com
License:	GPL
Group:		Development/System
Buildroot:	%{_tmppath}/%{name}-%{version}-root
Prefix:		%{_stm_install_prefix}
Source:		stmfb.tar.bz2

#PATCH_DEFINITIONS

%description
Source code for the STM Frame Buffer driver. This allows drivers to be built
for the frame buffer on the STx7105, STx7106, STx7108, STx7111, STx7141
and STx7200.

%prep
%setup -qn stmfb-%{version} -c stmfb-%{version}

#PATCH_IMPLEMENTATION

%build
# Well, that was easy!

%install
install -d -m 755 %{buildroot}%{_stm_sources_dir}/stmfb/stmfb-%{version}
tar chf - . | tar xf - -C %{buildroot}%{_stm_sources_dir}/stmfb/stmfb-%{version}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%dir %{_stm_sources_dir}/stmfb
%{_stm_sources_dir}/stmfb/stmfb-%{version}

%post
rm -f %{_stm_sources_dir}/stmfb/stmfb
ln -s stmfb-%{version} %{_stm_sources_dir}/stmfb/stmfb

%preun
if [ x`readlink %{_stm_sources_dir}/stmfb/stmfb` = xstmfb-%{version} ] ; then
  rm -f %{_stm_sources_dir}/stmfb/stmfb
fi

%changelog
* Fri Apr 5  2013 Andrew Watkins <andrew.watkins@mathembedded.com> 
- [Spec] Prepare release including patch files
- Added a configurable delay between VTG clocks
- Add rpm Makefile, spec file, and source directory
- Added a pre OSD/post OSD capability, added code to sync VTG clocks
- Inclusion of SCART path configuration application
- stmfb-3.1_0032-v4l2_buf_private
- stmfb_fusion_preemphasis_dac
- stmfb_bug9461
- disable_stx7106_and_08
- stgfb-bug5044-suppress-WS
- stgfb-bug5574
- configurable-hardware
- genbox
- ignore-bios
* Mon Jun 14 2010 André Draszik <andre.draszik@st.com> 3.1_stm23_0032 (Final checkin)
* Fri Mar 12 2010 André Draszik <andre.draszik@st.com> 3.1_stm23_0029-1
- [Spec] let's own ${_stm_sources_dir}/stmfb as well
- [Bugzilla: 8298] fix build for CONFIG_PM case
- [Add patch: stmfb-3.1_stm23_0029_Bug8298_buildfix_CONFIG_PM.patch] fix build
  for CONFIG_PM case
- [Bugzilla: 7993] API required to disable video plane in the mixer
- [Bugzilla: 7995] APIs needed for changing screen & plane config using DirectFB
- [Bugzilla: 8065] allow more pixelformats on framebuffer plane
- a crash could occur in var_ext handling on framebuffer devices without HDMI
- remove all internal references to any 'aux' output
- [Bugzilla: 8193] Framebuffer panning and GDP immediate buffer address updates
   NOTE: This changes the semantics of the DirectFB1.0 flip flags hence will
   effect the behaviour of existing applications.
- [Bugzilla: 8213] replace FBIO_GETVSYNC_ELAPSED with standard FBIOGET_VBLANK
  framebuffer ioctl

* Sat Dec 19 2009 André Draszik <andre.draszik@st.com> 3.1_stm23_0028-1
- [Bugzilla: 7098] Add 7106 Linux support. NOTE: Requires kernel
  linux-sh4-2.6.23.17_stm23_0123 with 7106 support.
- [Bugzilla: 7123] Improve 64/64bit division handling in stmfb
- [Bugzilla: 7308] Compatibility of HDMI output
- [Bugzilla: 7325] Complete 7108 Linux support
- [Bugzilla: 7469] Add support for a NULL display plane
- [Bugzilla: 7514] A race condition exists in the mixer class
- [Bugzilla: 7834] FMD hardware processing can be disabled when not needed
- [Bugzilla: 7835] allow to set HD DEI configuration using Plane Control
- [Bugzilla: 7836] implement various V4L2 controls for modifying FMD thresholds
- [Bugzilla: 7332] stgfx2 DirectFB driver should be default when possible

* Mon Oct 19 2009 André Draszik <andre.draszik@st.com> 3.1_stm23_0027-1
- [Bugzilla: 6773] Re-factoring exercise
- [Bugzilla: 6826] kernel oops when unloading stmfb
- [Bugzilla: 6828] problems enumerating V4L2 devices
- [Bugzilla: 6870] Add TMDS status to hdmi-info
- [Bugzilla: 6890] Display driver fails to initialize on MB680 with kernel 121
- [Bugzilla: 6937] Add provisional support for STx5206/5289
- [Bugzilla: 6938] Integrate board support for IPIDTV 7105 platform
- [Bugzilla: 6958] RGB Analogue output on 7105 has incorrect red voltage
- [Bugzilla: 6971] Add preliminary support for 7108/MB837
- [Bugzilla: 7040] empty blit node crashes BDisp
- [Bugzilla: 7041] BDisp: always filter when doing stretch blits
- [Bugzilla: 7047] A STi7200 HDMI Phy macro definition is incorrect
- [Bugzilla: 7048] BDisp: vertical filter should not have an initial phase
- [Bugzilla: 7159] Grey Screen of Death
- [Bugzilla: 7268] don't modify display buffers when FlexVP (TNR) is in use
- various HDCP related fixes
- add BIOS support

* Mon Jul  7 2009 André Draszik <andre.draszik@st.com> 41
- [Update: 3.1_stm23_0026] Upgraded to latest release.
- [Bugzilla: 6365] Support scaling specific GDP filter coefficients
- [Bugzilla: 6372] Lock access to BDisp shared area when necessary
- [Bugzilla: 6542] No sanity check in CalculatingNonLinearZoom
- [Bugzilla: 6581] STx7106 and Deepcolour HDMI support

* Mon Jun  1 2009 André Draszik <andre.draszik@st.com> 40
- [Update: 3.1_stm23_0025] Upgraded to latest release.
- [Bugzilla: 5822] Add an interface to force the display output YCbCr
  colourspace
- [Bugzilla: 5823] Update graphics planes to support video playback.
  WARNING: this changes the default buffer handling on interlaced displays,
  V4L2 users should read the bug to see the impact and how to get back to the
  previous behaviour.
- [Bugzilla: 5831] correctly implement hide and show of video planes
- [Bugzilla: 5834] Always use de-interlacer (when available and enabled) for
  interlaced output.
- [Bugzilla: 5844] Remove V4L2_BUF_FLAG_BOTTOM_FIELD_FIRST
- [Bugzilla: 5845] Fully implement display standard APIs for V4L2 output devices
- [Bugzilla: 5879] v4l2: when queuing MMAP buffers CLUT is not considered
- [Bugzilla: 5899] PAL/NTSC output on main display pipeline sometimes has bad
  sync
- [Bugzilla: 5919] HDMI hotplug misbehaving on SDK-7105 board.
- [Bugzilla: 5929] HDMI connection status can be confused by short
  hotplug pulses
- [Bugzilla: 5932] STMHDMIIO_SET_SAFE_MODE_PROTOCOL locking broken
- [Bugzilla: 5935] STMHDMIIO_SET_EDID_MODE_HANDLING should take immediate effect
- [Bugzilla: 5956] Temporal deinterlacing produces a bad frame on scene change
- [Bugzilla: 5975] Allow HDMI device to be disabled on module load
- [Bugzilla: 5888] GDP and Video layers are not aligned properly when rescaled
- [Bugzilla: 6095] need to re init v4l2 buffer list nodes on VIDIOC_STREAMOFF
- [Bugzilla: 6006] V4L2 Sliced VBI Output for EBU Teletext
- [Bugzilla: 6113] Should use rounded division to calculate N
- [Bugzilla: 6148] Allow other hardware drivers to hook into the primary vsync
  interrupt handler
- [Bugzilla: 6169] HDMI vertical active area is incorrect for 480i
- [Bugzilla: 6294] Video pipeline hardware accessing memory when inactive, 
  but with bypass enabled.
- [Bugzilla: 6298] Support for graphics buffers with an alpha range of 0-128

* Thu Apr 23 2009 André Draszik <andre.draszik@st.com> 40
- [Spec] remove bashisms

* Wed Mar  6 2009 André Draszik <andre.draszik@st.com> 39
- force a rebuild to pick up fixed archive

* Wed Mar  4 2009 André Draszik <andre.draszik@st.com> 39
- [Update: 3.1_stm23_0024] Upgraded to latest release.
- [Bugzilla: 5796] finish implementation of V4L2 controls for color keying
  Note: Please recompile your applications, binary compatibility has been
  broken.
- [Bugzilla: 5801] PLANE_CTRL_CAPS_PSI_CONTROLS is never set in plane
  control caps
- [Bugzilla: 5598] v4l2 video plug controls for PSI
- [Bugzilla: 5799] add macroblock blitting support to BDisp driver

* Thu Feb 26 2009 André Draszik <andre.draszik@st.com> 38
- [Update: 3.1_stm23_0023] Upgraded to latest release.
- NEW: core driver support for pixel aspect ratio correction, preserving the
  original image aspect ratio on 16:9 displays, in a defined central region.
- NEW: main DVO support on 7111/7141/7105
- [Bugzilla: 5449] 7200cut2 1080i DVP capture and main video pipeline display
  artifacts
- [Bugzilla: 5453] Delay EDID read retries
- [Bugzilla: 5497] Add new output control STM_CTRL_422_CHROMA_FILTER, currently
  implemented on DVO output on 7200 and later chips to optionally decimate
  chroma from 4:4:4 to 4:2:2 with a 1:2:1 three tap filter.
- [Bugzilla: 5531] Add stmfb board support for 7200cut3.
- [Bugzilla: 5587] Add an override to allow the EDID safe mode to use HDMI
  instead of DVI.
- [Bugzilla: 5588] Use readl/writel etc. for accessing ioreamp()ed registers,
  instead of ctrl_inl/ctrl_outl etc.
- [Bugzilla: 5608] Add coredisplay driver support for PDK7105 board.
- [Bugzilla: 5631] Add scaling limits to the display plane capabilities
  structure
- [Bugzilla: 5646]  Remove HDMI rawedid sysfs attribute
- [Bugzilla: 5655] TV EDID being re-read unnecessarily
- [Bugzilla: 5683] Audio InfoFrames cannot be queued for transmission
- [Bugzilla: 5704] Improve Interlaced Video Trick Modes with DEI and P2I
- [Bugzilla: 5752] 7109 De-interlacer only supports sources up to 720
  pixels wide
- [Bugzilla: 5142] homogenise v4l2 drivers
- [Bugzilla: 4961] add definition of upcoming V4L2 controls for color keying
- [Spec] main directory in stmfb archive is expected to be stmfb-%%{version}

* Wed Jan  7 2009 Stuart Menefy <stuart.menefy@st.com> 37
- [Update: 3.1_stm23_0022] Upgraded to latest release.
- [Bugzilla: 5311] APIs required to hide and show video planes.

* Fri Nov 28 2008 Stuart Menefy <stuart.menefy@st.com> 36
- [Update: 3.1_stm23_0021] Upgraded to latest release.
- WARNING: The HDMI InfoFrame mechanism has been completely rewritten so that
  new hardware can be accomodated and so that the current hardware can be 
  programmed using a DMA engine instead of the CPU. Do not be suprised to find
  bugs despite our best efforts; if you do not want to experiment stay with
  stmfb-3.0_stm23_0020.
- CHANGE: The HDMI device interface for sending various InfoFrames and other
  data island packets has changed. ISRC packets and a usable mechanism for
  queuing multiple Vendor specific InfoFrames is now available.
- NEW: The IOCTL STMHDMIIO_SET_HOTPLUG_MODE has been added to the HDMI device,
  allowing one of 3 different behaviours when a hotplug de-assert is detected.
  See Linux/stm/coredisplay/stmhdmi.h for details.
- NEW: The IOCTL STMHDMIIO_SET_CEA_MODE_SELECTION has been added to the HDMI
  device. This allows the method, by which the CEA mode number in the AVI
  InfoFrame for modes that support 4:3 and 16:9 variants, is selected. See
  Linux/stm/coredisplay/stmhdmi.h for details.
- [Bugzilla: 4881] Change HDMI SPD InfoFrame transmission rate to every
  1/2 second.
- [Bugzilla: 4943] HD output over HDMI is broken since -0019 on 7109c3.
- [Bugzilla: 4994] Fix IOCTL argument copying sizes.
- [Bugzilla: 5049] Analogue WSS defaults to 4x3 instead of NO_AFD
- [Bugzilla: 5072] Incorrect HDMI vsync lengths in interlaced display modes
- [Bugzilla: 5090] stmfb build system doesn't allow to override CROSS_COMPILE
- [Bugzilla: 5094] Component analogue sync generation for 1080i modes incorrect
- [Bugzilla: 5106] Make TMDS clock status available to userspace
- [Bugzilla: 5122] 7109 HDMI output is horizontally shifted to the right by
  several pixels
- [Bugzilla: 5161] HDMI AVI InfoFrames no longer indicate a 709 colourspace for
  HD modes
- [Bugzilla: 5164] MB519 Analogue YPrPb output disappears after display mode
  change
- [Bugzilla: 5180] Review of video output alignments required
- [Bugzilla: 5216] Definition of HDMI_AVI_INFOFRAME_NOSCANDATA is incorrect.

* Wed Aug 20 2008 Stuart Menefy <stuart.menefy@st.com> 35
- [Update: 3.0_stm23_0020] Upgraded to latest release.
- [Bugzilla: 4734] Computer Monitor EDIDs may be incorrectly treated as invalid.
- [Bugzilla: 4740] Framebuffer fails to set 1080p@59.94Hz
- [Bugzilla: 4757] driver fails to load on STi7105
- [Bugzilla: 4771] Support different plane queue sizes for different plane types

* Wed Aug 20 2008 Stuart Menefy <stuart.menefy@st.com> 34
- [Update: 3.0_stm23_0019] Upgraded to latest release.
- NEW: Added support for accessing the BDisp directly from userspace
- CHANGE: Instead of sharing one BDisp AQ between all the video output
  pipelines, use a different AQ for every output on platforms that
  support this.
- [Bugzilla: 4583] Changes requested to HDMI EDID block0/1 error handling
- [Bugzilla: 4653] More robust reading of EDID extension blocks.
- [Bugzilla: 4683] Support for HDMI quad pixel repeated modes required
  for Dolby TrueHD certification.
- [Bugzilla: 4689] RLE hardware decoding locks BDisp on some source data
- [Bugzilla: 4694] Show output IDs in display pipeline's "info" sysfs attribute.
- [Bugzilla: 4706] Aux SD YPrPB output disabled when main HD display mode
  changed.
- [Bugzilla: 4711] Video range clipping may not be correct on Aux display
  SD YPrPb output on 7111,7105,7141 and 7200cut2.
- [Bugzilla: 4724] Progressive planar and 422 raster formatted video
  incorrectly presented.

* Wed Aug 20 2008 Stuart Menefy <stuart.menefy@st.com> 33
- [Update: 3.0_stm23_0018] Upgraded to latest release.
- NEW: Added status information to display planes, this is available both in
  the buffer completed callback status information and via
  stm_display_plane_get_status().
- [Bugzilla: 4503] Fix incorrect reading of CEA block DTD EDID entries.
- [Bugzilla: 4506]: colorkey was incorrectly calculated for the ARGB1555 format
- [Bugzilla: 4536] HDMI output unreliable on Sony KDL-40V3000

* Tue Aug 12 2008 Stuart Menefy <stuart.menefy@st.com> 32
- [Update: 3.0_stm23_0017] Upgraded to latest release.
- NEW: Support for STx7105.
- NEW: Support for STi5202/MB602.
- NEW: Support for HDMI IT content information in the AVI InfoFrame via a new
  HDMI device IOCTL STMHDMIIO_SET_CONTENT_TYPE.
- NEW: Added output API function stm_display_output_soft_reset() which when
  called on master outputs will reset the timing generator causing a top
  field vsync to be issued immediately. To be used to do a coarse adjustment
  of the output vsync relative to the input vsync of captured video.
- NEW: HDMI safe mode when EDID is unreadable, as documented in CEA-861-E
  section F.3.5. The safe mode reports the sink as DVI supporting 640x480p.
- NEW: Added new HDMI device IOCTL STMHDMIIO_SET_EDID_MODE_HANDLING; this 
  changes the behaviour of the HDMI management thread, allowing HDMI to 
  transmit display modes not listed in the connected sink's EDID. The
  hdmi-control utility has the related new option --edid-mode .
- NEW: Added output API function stm_display_output_set_filter_coefficients()
  which can be used to change the DENC luma and chroma filters (previous 
  control based mechanism has been removed) and the component analogue DAC
  sample rate converter filters on 7111/7105/7141 and 7200cut2.
- CHANGE: Updated HDMI EDID parsing and reporting in sysfs for CEA-861-E.
- CHANGE: Do not set the HDMI AVI InfoFrame RGB quantization bits unless
  the corresponding QS bit in the TV's EDID VCDB block has been set and we
  are outputting RGB.
- CHANGE: HDMI AVI InfoFrame under/over scan information now supports setting
  the "not known" value, a new HDMI device IOCTL STMHDMIIO_SET_OVERSCAN_MODE and
  argument type stmhdmiio_overscan_mode_t has been added; the previous IOCTL
  STMHDMIIO_SET_VIDEO_SCAN_TYPE has been removed.
- CHANGE: support 1080p50/60 on analogue out where hardware is capable
- [Bugzilla: 4347] Fix 7200cut2 HDMI/SPDIF configuration error.
- [Bugzilla: 4435] Fix HDMI AVI InfoFrame colorimetry bits when outputting RGB.

* Mon Jul 14 2008 Stuart Menefy <stuart.menefy@st.com> 31
- [Update: 3.0_stm23_0016] Upgraded to latest release.
- NEW: Add support for HDMI ACP packets
- NEW: Added /sys/class/stmcoredisplay/info attribute which reports useful
  information about the driver build. Use this to determine if you are running
  the driver version you think you are running before reporting bugs.
- NEW: Support soruce video crop coordinates in 1/32nd of a pixel as well as
  1/16th and whole pixels.
- NEW: Support for STx7141, it has exactly the same feature set as STi7111.
- CHANGE: 1080i de-interlacing using full conf motion mode with 8bit motion
  buffers provoked memory bandwidth artifacts. This change selects the reduced
  4bit motion mode for content width than 960 pixels and appears to eliminate
  the artifacts.
- CHANGE: allow HW de-interlacing for all allowable downscales when displaying
  on an interlaced display. This allows better quality 1080i->PAL/NTSC
  conversion. When doing SD->1/4SD scaling, for example as part of an EPG,
  de-interlacing can always be explicitly turned off using the plane controls
  if required.
- CHANGE: the fixed point maths that calculates the HW sample rate converter
  steps when doing a stretch blit has been changed to round the result instead
  of doing a "floor". This minimises the absolute pixel position error that can
  be introduced by the 10bit fixed point maths used by the HW.
- CHANGE: on the 7200cut2, de-interlacer motion buffers (approximately 3Mb) are
  now allocated from the bigphysarea partition instead of the coredisplay-video
  partition. This is to balance memory bandwidth usage while processing 1080i
  video.
- [Bugzilla: 3740] fix subpixel position register programming in BDisp nodes 
  when generating a stretchblit operation that requires multiple spans.
- [Bugzilla: 4330] set the current output format when starting the auxillary
  display pipeline. This now correctly configures YPrPb/RGB output via the
  HD DACs on all SoCs that support "SCART mode" on the auxillary display.

* Wed Jun 25 2008 Stuart Menefy <stuart.menefy@st.com> 30
- [Update: 3.0_stm23_0015] Upgraded to latest release.
- NEW: Add preliminary support for 7200cut2.
- NEW: Add support for HDMI YUV 422 output.
- NEW: Add DVO output format configuration support to STMFBIO_SET_OUTPUT_CONFIG
  ioctl and stfbset.
- NEW: Hardware progressive to interlaced conversion enabled on 7111 and
  7200cut2 main video pipelines, allowing the full range of the vertical 
  rescaler to be used in all circumstances.
- NEW: De-interlacing of 1080i content enabled on 7111 and 7200cut2.
- NEW: IQI supported on 7111
- NEW: AWG firmware filename changes. (See stmcoredisplay.txt for updated
  modprobe.conf)
- NEW: Support 8 channel HDMI audio.
- NEW: Allow HDMI audio processing to be disabled and audio packets to be 
       invalidated.
- NEW: Add support for all HDMI audio packet types (1-bit,DST,HBR), supported
       on 7111 and 7200Cut2.
- NEW: add DEI plane controls
- NEW: yuvplayer test program to play back raw yuv (video) streams.
- [Bugzilla: 3908] Issues with 59/60Hz only sinks and fast mode changes. It
  has been necessary to disable fast mode changes between 50Hz and 59.94/60Hz
  modes to avoid problems with HDMI on US TV's that do not support 50Hz.
- [Bugzilla: 4004] Support new config name for HMS1 board in -0111 kernel.
- [Bugzilla: 4005] Fix simultaneous SD SCART and HD HDMI output on 7111.
- [Bugzilla: 4006] Rework VTG setup for the DENC in 7200 and 7111.
- [Bugzilla: 3906] Make switching video modes more reliable on 710x/HDMI.
  There seems to be an issue in the HDMI cell, making the serializer fail
  sometimes. A work-around has been found by experimentation which has
  been successfully tested on 7100c3.3, 7109c3.0 & 7109c3.2
- [Bugzilla: 4107] Fix the video filter coefficient loading logic such that
  multiple instances of the class do not interfere with each other.
- [Bugzilla: 4165] same as 3906 but for STi7200.
- [Bugzilla: 4224] cope with 64MB banks in blitter

* Fri Apr 25 2008 Stuart Menefy <stuart.menefy@st.com> 29
- [Update: 0014] Upgraded to latest release.

* Wed Mar 19 2008 Stuart Menefy <stuart.menefy@st.com> 28
- [Update: 0013] Upgraded to latest release.

* Mon Jan 27 2008 Stuart Menefy <stuart.menefy@st.com> 27
- [Update: 0012] Upgraded to latest release.

* Thu Dec 13 2007 Stuart Menefy <stuart.menefy@st.com> 26
- Upgraded to 0011 release.

* Fri Nov  9 2007 Stuart Menefy <stuart.menefy@st.com> 25
- Upgraded to 0010 release.

* Fri Nov  2 2007 Stuart Menefy <stuart.menefy@st.com> 24
- Upgraded to 0009 release.

* Wed Aug  1 2007 Stuart Menefy <stuart.menefy@st.com> 23
- Initial version, based on stgfb-2.0_stm22_0006-22 .spec file.
