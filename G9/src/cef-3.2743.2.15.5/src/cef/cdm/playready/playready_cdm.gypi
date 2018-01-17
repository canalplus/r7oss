{
  'conditions': [
    ['enable_pepper_cdms==1 and use_playready==1', {
      'targets': [
        {
          'target_name': 'playreadycdm',
          'type': 'none',
          'conditions': [
            ['os_posix == 1 and OS != "mac"', {
              'type': 'loadable_module',  # Must be in PRODUCT_DIR for ASAN bot.
              'cflags': [
                '<!@(<(pkg-config) --cflags playready-2.5_drm)',
              ],
              'libraries': [
                '<!@(<(pkg-config) --libs playready-2.5_drm)',
              ],
              'conditions': [
                # PlayReady built with 2-byte wchar_t's. That causes lots of
                # warnings at link time as wchar_t's default to 4 bytes. This
                # option is available on ARM only.
                ['target_arch=="arm"', {
                  'ldflags': ["-Wl,--no-wchar-size-warning"]
                }],
              ],
            }],
            ['OS == "mac" or OS == "win"', {
              'type': 'shared_library',
            }],
            ['OS == "mac"', {
              'xcode_settings': {
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
              },
            }]
          ],
          'defines': ['CDM_IMPLEMENTATION'],
          'dependencies': [
            'media',
            '../url/url.gyp:url_lib',
            # Include the following for media::AudioBus.
            'shared_memory_support',
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            '<(DEPTH)/media/cdm/ppapi/cdm_logging.cc',
            '<(DEPTH)/media/cdm/ppapi/cdm_logging.h',
            'playready_cdm.cc',
            'playready_cdm.h',
            'playready_keysession.h',
            'playready_keysession.cc',
            'playready_types.h',
            'playready_types.cc',
            'playready_utils.h',
            'playready_utils.cc',
            'playready_datastore.h',
            'playready_datastore.cc',
          ],
        },
        {
          'target_name': 'playreadycdmadapter',
          'type': 'none',
          # Check whether the plugin's origin URL is valid.
          'defines': ['CHECK_DOCUMENT_URL'],
          'dependencies': [
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
            '<(DEPTH)/media/media_cdm_adapter.gyp:cdmadapter',
            'playreadycdm',
          ],
          'conditions': [
            ['os_posix == 1 and OS != "mac"', {
              # Because playreadycdm has type 'loadable_module' (see comments),
              # we must explicitly specify this dependency.
              'libraries': [
                # Built by playreadycdm.
                '<(PRODUCT_DIR)/libplayreadycdm.so',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
