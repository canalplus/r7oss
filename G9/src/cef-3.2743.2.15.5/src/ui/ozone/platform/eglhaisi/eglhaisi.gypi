# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_eglhaisi',
    ],
    'internal_ozone_platforms': [
      'eglhaisi'
    ],
    # Must be set. Supported values are directfb or nexus.
    'ozone_platform_eglhaisi_backend%': 'directfb',
    # Nexus specific variable for zorder of graphical plan
    'ozone_platform_eglhaisi_nexus_zorder%': '0',
  },
  'targets': [{
    'target_name': 'ozone_platform_eglhaisi',
    'type': 'static_library',
    'defines': [
      'OZONE_IMPLEMENTATION',
    ],
    'dependencies': [
      'ozone.gyp:ozone_base',
      'ozone.gyp:ozone_common',
      '../../base/base.gyp:base',
      '../events/events.gyp:events',
      '../events/ozone/events_ozone.gyp:events_ozone_evdev',
      '../gfx/gfx.gyp:gfx',
    ],
    'conditions': [
      ['ozone_platform_eglhaisi_backend=="directfb"', {
        'defines': ['OZONE_PLATFORM_EGLHAISI_DIRECTFB=1'],
        'cflags': [
          '<!@(pkg-config --cflags glesv2 directfb)',
        ],
        'link_settings': {
          'libraries': [
            # We don't need to link with EGL or GLES libraries as they are
            # dynamically loaded.
            '<!@(pkg-config --libs directfb)',
          ],
        },
      }],
      ['ozone_platform_eglhaisi_backend=="nexus"', {
        'defines': [
	  'OZONE_PLATFORM_EGLHAISI_NEXUS=1',
	  'OZONE_PLATFORM_EGLHAISI_NEXUS_ZORDER=<(ozone_platform_eglhaisi_nexus_zorder)'
	],
        'cflags': [
          '<!@(pkg-config pkg-config --cflags nexus)',
        ],
        'link_settings': {
          'libraries': [
            # We need to link with the v3driver library even if it's
            # dynamically loaded as the nxpl library needs symbols from it.
            '-lnxpl',
            '-lv3ddriver',
            '<!@(pkg-config --libs nexus)',
          ],
        },
      }],
      ['ozone_platform_eglhaisi_backend=="mstar"', {
        'defines': ['OZONE_PLATFORM_EGLHAISI_MSTAR=1'],
        'cflags': [
          '<!@(pkg-config pkg-config --cflags mi glesv2)',
        ],
        'link_settings': {
          'libraries': [
            # We don't need to link with EGL or GLES libraries as they are
            # dynamically loaded.
            '-lz',
            '<!@(pkg-config --libs mi directfb)',
          ],
        },
      }],
    ],
    'sources': [
      'ozone_platform_eglhaisi.cc',
      'ozone_platform_eglhaisi.h',
      'client_native_pixmap_factory_eglhaisi.cc',
      'client_native_pixmap_factory_eglhaisi.h',
      'eglhaisi_surface_factory.cc',
      'eglhaisi_surface_factory.h',
    ],
  }],
}
