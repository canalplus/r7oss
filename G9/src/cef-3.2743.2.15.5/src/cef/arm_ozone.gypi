# Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'proprietary_codecs': 1,
    'v8_use_snapshot': 'true',
    'linux_use_tcmalloc': 1,
    'linux_link_kerberos': 0,
    'use_kerberos': 0,
    'use_gconf' : 0,
    'use_aura' : 1,
    'use_ozone' : 1,
    'embedded' : 1,
    'enable_plugins' : 1,
    'enable_webrtc' : 1,
    'use_pulseaudio' : 0,
    'use_dbus' : 0,
    'include_tests' : 0,
    'toolkit_views' : 1,
    'use_cups' :0,
    'use_gnome_keyring': 0,
    'use_openssl' : 1,
    'target_arch' : 'arm',
    'arm_float_abi' : 'softfp',
    'arm_neon' : 1,
    'arm_fpu': 'neon',
    'arm_tune' : 'cortex-a7',
    'ozone_platform' : 'eglhaisi',
    'ozone_platform_eglhaisi' : 1,
    'use_x11' : 0,
    'linux_link_libpci' : 0,
    'use_libpci' : 0,
    'ffmpeg_branding' : 'Chrome',
    'video_hole' : 1,
  },
}
