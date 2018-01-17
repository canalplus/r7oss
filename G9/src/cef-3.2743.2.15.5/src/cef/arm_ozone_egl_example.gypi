# This is an example configuration to build CEF for ARM using Ozone/EGL as
# the rendering engine. You MUST set hardware-platform-related variables to
# match your board and toolchain configuration.

{
  'variables': {

    # Hardware platform.
    #
    # You MUST adjust to your board/toolchain.
    'target_arch'   : 'arm',
    'arm_float_abi' : 'hard',
    'arm_neon'      : 1,
    'arm_fpu'       : 'neon',
    'arm_tune'      : 'cortex-a7',

    # Integration with the system.
    #
    # You should not need to edit them.
    'use_pulseaudio'     : 0,
    'use_gconf'          : 0,
    'use_aura'           : 1,
    'use_ozone'          : 1,
    'use_kerberos'       : 0,
    'use_libpci'         : 0,
    'use_cups'           : 0,
    'use_gnome_keyring'  : 0,
    'use_openssl'        : 1,
    'use_openssl_certs'  : 1,
    'use_nss_certs'      : 0,
    'use_x11'            : 0,
    'use_dbus'           : 0,
    'use_pango'          : 0,
    'use_cairo'          : 0,
    'use_system_libexif' : 0,

    # Features.
    'disable_nacl'           : 1,
    'disable_nacl_untrusted' : 1,
    'disable_pnacl'          : 1,
    'enable_plugins'         : 1,
    'enable_webrtc'          : 0,

    # Build environment.
    'embedded'                      : 1,
    'sysroot'                       : '',
    'werror'                        : '',
    'disable_fatal_linker_warnings' : 1,
    'release_unwind_tables'         : 0,

    # Ozone platform configuration.
    #
    # You MUST select the right backend for your platform by setting the
    # ozone_platform_eglhaisi_backend variable. Possible values: directfb or
    # nexus.
    'ozone_platform'                  : 'eglhaisi',
    'ozone_platform_eglhaisi'         : 1,
    'ozone_platform_egltest'          : 0,
    'ozone_platform_headless'         : 0,
    'ozone_platform_eglhaisi_backend' : 'directfb',

    # Linux integration.
    #
    # You should not need to edit them.
    'linux_use_tcmalloc'         : 1,
    'linux_link_kerberos'        : 0,
    'linux_link_libpci'          : 0,
    'linux_use_bundled_binutils' : 0,
    'linux_use_bundled_gold'     : 0,
    'linux_use_gold_flags'       : 0,

    # Audio/video support.
    #
    # You should select the CDM you want to have support for. You can also
    # choose to disable proprietary codecs.
    'proprietary_codecs' : 1,
    'ffmpeg_branding'    : 'Chrome',
    'video_hole'         : 1,
    'enable_widevine'    : 0, # Widevine CDM.
    'use_playready'      : 0, # PlayReady CDM.

    # Miscellaneous.
    #
    # You should not need to edit them.
    'v8_use_snapshot' : 'true',
    'clang'           : 0,
    'host_clang'      : 0,
    'include_tests'   : 0,
    'toolkit_views'   : 1,
  },
}

