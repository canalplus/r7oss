dnl ALSA soundcard configuration
dnl Find out which cards to compile driver for
dnl Copyright (c) by Jaroslav Kysela <perex@perex.cz>,
dnl                  Anders Semb Hermansen <ahermans@vf.telia.no>

AC_DEFUN([ALSA_TOPLEVEL_INIT], [
	CONFIG_HAS_IOMEM=""
	CONFIG_SOUND=""
	CONFIG_SND=""
	CONFIG_SND_TIMER=""
	CONFIG_SND_PCM=""
	CONFIG_SND_HWDEP=""
	CONFIG_SND_RAWMIDI=""
	CONFIG_SND_SEQUENCER=""
	CONFIG_SND_SEQ_DUMMY=""
	CONFIG_SND_OSSEMUL=""
	CONFIG_SND_MIXER_OSS=""
	CONFIG_SND_PCM_OSS=""
	CONFIG_SND_PCM_OSS_PLUGINS=""
	CONFIG_SND_SEQUENCER_OSS=""
	CONFIG_SND_RTCTIMER=""
	CONFIG_RTC=""
	CONFIG_SND_SEQ_RTCTIMER_DEFAULT=""
	CONFIG_SND_DYNAMIC_MINORS=""
	CONFIG_SND_SUPPORT_OLD_API=""
	CONFIG_SND_VERBOSE_PROCFS=""
	CONFIG_PROC_FS=""
	CONFIG_SND_VERBOSE_PRINTK=""
	CONFIG_SND_DEBUG=""
	CONFIG_SND_DEBUG_DETECT=""
	CONFIG_SND_PCM_XRUN_DEBUG=""
	CONFIG_SND_BIT32_EMUL=""
	CONFIG_SND_DEBUG_MEMORY=""
	CONFIG_SND_HPET=""
	CONFIG_HPET=""
	CONFIG_SND_MPU401_UART=""
	CONFIG_SND_OPL3_LIB=""
	CONFIG_SND_OPL4_LIB=""
	CONFIG_SND_VX_LIB=""
	CONFIG_SND_AC97_CODEC=""
	CONFIG_SND_DUMMY=""
	CONFIG_SND_VIRMIDI=""
	CONFIG_SND_MTPAV=""
	CONFIG_SND_MTS64=""
	CONFIG_PARPORT=""
	CONFIG_SND_SERIAL_U16550=""
	CONFIG_SND_MPU401=""
	CONFIG_SND_PORTMAN2X4=""
	CONFIG_SND_ML403_AC97CR=""
	CONFIG_XILINX_VIRTEX=""
	CONFIG_SND_SERIALMIDI=""
	CONFIG_BROKEN=""
	CONFIG_SND_LOOPBACK=""
	CONFIG_SND_PCSP=""
	CONFIG_EXPERIMENTAL=""
	CONFIG_X86_PC=""
	CONFIG_HIGH_RES_TIMERS=""
	CONFIG_SND_AD1848_LIB=""
	CONFIG_SND_CS4231_LIB=""
	CONFIG_SND_SB_COMMON=""
	CONFIG_SND_SB8_DSP=""
	CONFIG_SND_SB16_DSP=""
	CONFIG_ISA=""
	CONFIG_ISA_DMA_API=""
	CONFIG_SND_ADLIB=""
	CONFIG_SND_AD1816A=""
	CONFIG_PNP=""
	CONFIG_ISAPNP=""
	CONFIG_SND_AD1848=""
	CONFIG_SND_ALS100=""
	CONFIG_SND_AZT2320=""
	CONFIG_SND_CMI8330=""
	CONFIG_SND_CS4231=""
	CONFIG_SND_CS4232=""
	CONFIG_SND_CS4236=""
	CONFIG_SND_DT019X=""
	CONFIG_SND_ES968=""
	CONFIG_SND_ES1688=""
	CONFIG_SND_ES18XX=""
	CONFIG_SND_SC6000=""
	CONFIG_HAS_IOPORT=""
	CONFIG_SND_GUS_SYNTH=""
	CONFIG_SND_GUSCLASSIC=""
	CONFIG_SND_GUSEXTREME=""
	CONFIG_SND_GUSMAX=""
	CONFIG_SND_INTERWAVE=""
	CONFIG_SND_INTERWAVE_STB=""
	CONFIG_SND_OPL3SA2=""
	CONFIG_SND_OPTI92X_AD1848=""
	CONFIG_SND_OPTI92X_CS4231=""
	CONFIG_SND_OPTI93X=""
	CONFIG_SND_MIRO=""
	CONFIG_SND_SB8=""
	CONFIG_SND_SB16=""
	CONFIG_SND_SBAWE=""
	CONFIG_SND_SB16_CSP=""
	CONFIG_PPC=""
	CONFIG_FW_LOADER=""
	CONFIG_SND_SB16_CSP_FIRMWARE_IN_KERNEL=""
	CONFIG_SND_SGALAXY=""
	CONFIG_SND_SSCAPE=""
	CONFIG_SND_WAVEFRONT=""
	CONFIG_SND_WAVEFRONT_FIRMWARE_IN_KERNEL=""
	CONFIG_SND_PC98_CS4232=""
	CONFIG_X86_PC9800=""
	CONFIG_SND_MSND_PINNACLE=""
	CONFIG_X86=""
	CONFIG_PCI=""
	CONFIG_SND_AD1889=""
	CONFIG_SND_ALS300=""
	CONFIG_SND_ALS4000=""
	CONFIG_SND_ALI5451=""
	CONFIG_SND_ATIIXP=""
	CONFIG_SND_ATIIXP_MODEM=""
	CONFIG_SND_AU8810=""
	CONFIG_SND_AU8820=""
	CONFIG_SND_AU8830=""
	CONFIG_SND_AZT3328=""
	CONFIG_SND_BT87X=""
	CONFIG_SND_BT87X_OVERCLOCK=""
	CONFIG_SND_CA0106=""
	CONFIG_SND_CMIPCI=""
	CONFIG_SND_OXYGEN_LIB=""
	CONFIG_SND_OXYGEN=""
	CONFIG_SND_CS4281=""
	CONFIG_SND_CS46XX=""
	CONFIG_SND_CS46XX_NEW_DSP=""
	CONFIG_SND_CS5530=""
	CONFIG_SND_CS5535AUDIO=""
	CONFIG_X86_64=""
	CONFIG_SND_DARLA20=""
	CONFIG_SND_GINA20=""
	CONFIG_SND_LAYLA20=""
	CONFIG_SND_DARLA24=""
	CONFIG_SND_GINA24=""
	CONFIG_SND_LAYLA24=""
	CONFIG_SND_MONA=""
	CONFIG_SND_MIA=""
	CONFIG_SND_ECHO3G=""
	CONFIG_SND_INDIGO=""
	CONFIG_SND_INDIGOIO=""
	CONFIG_SND_INDIGODJ=""
	CONFIG_SND_EMU10K1=""
	CONFIG_SND_EMU10K1X=""
	CONFIG_SND_ENS1370=""
	CONFIG_SND_ENS1371=""
	CONFIG_SND_ES1938=""
	CONFIG_SND_ES1968=""
	CONFIG_SND_FM801=""
	CONFIG_SND_FM801_TEA575X_BOOL=""
	CONFIG_SND_FM801_TEA575X=""
	CONFIG_VIDEO_V4L1=""
	CONFIG_VIDEO_DEV=""
	CONFIG_SND_HDA_INTEL=""
	CONFIG_SND_HDA_HWDEP=""
	CONFIG_SND_HDA_CODEC_REALTEK=""
	CONFIG_SND_HDA_CODEC_ANALOG=""
	CONFIG_SND_HDA_CODEC_SIGMATEL=""
	CONFIG_SND_HDA_CODEC_VIA=""
	CONFIG_SND_HDA_CODEC_ATIHDMI=""
	CONFIG_SND_HDA_CODEC_CONEXANT=""
	CONFIG_SND_HDA_CODEC_CMEDIA=""
	CONFIG_SND_HDA_CODEC_SI3054=""
	CONFIG_SND_HDA_GENERIC=""
	CONFIG_SND_HDA_POWER_SAVE=""
	CONFIG_SND_HDA_POWER_SAVE_DEFAULT=""
	CONFIG_SND_HDSP=""
	CONFIG_SND_HDSPM=""
	CONFIG_SND_HIFIER=""
	CONFIG_SND_ICE1712=""
	CONFIG_SND_ICE1724=""
	CONFIG_SND_INTEL8X0=""
	CONFIG_SND_INTEL8X0M=""
	CONFIG_SND_KORG1212=""
	CONFIG_SND_KORG1212_FIRMWARE_IN_KERNEL=""
	CONFIG_SND_MAESTRO3=""
	CONFIG_SND_MAESTRO3_FIRMWARE_IN_KERNEL=""
	CONFIG_SND_MIXART=""
	CONFIG_SND_NM256=""
	CONFIG_SND_PCXHR=""
	CONFIG_SND_RIPTIDE=""
	CONFIG_SND_RME32=""
	CONFIG_SND_RME96=""
	CONFIG_SND_RME9652=""
	CONFIG_SND_SIS7019=""
	CONFIG_SND_SONICVIBES=""
	CONFIG_SND_TRIDENT=""
	CONFIG_SND_VIA82XX=""
	CONFIG_SND_VIA82XX_MODEM=""
	CONFIG_SND_VIRTUOSO=""
	CONFIG_SND_VX222=""
	CONFIG_SND_YMFPCI=""
	CONFIG_SND_YMFPCI_FIRMWARE_IN_KERNEL=""
	CONFIG_SND_AC97_POWER_SAVE=""
	CONFIG_SND_AC97_POWER_SAVE_DEFAULT=""
	CONFIG_SND_PDPLUS=""
	CONFIG_SND_ASIHPI=""
	CONFIG_SND_POWERMAC=""
	CONFIG_I2C=""
	CONFIG_INPUT=""
	CONFIG_PPC_PMAC=""
	CONFIG_SND_POWERMAC_AUTO_DRC=""
	CONFIG_PPC64=""
	CONFIG_PPC32=""
	CONFIG_SND_PS3=""
	CONFIG_PS3_PS3AV=""
	CONFIG_SND_PS3_DEFAULT_START_DELAY=""
	CONFIG_SND_AOA=""
	CONFIG_SND_AOA_FABRIC_LAYOUT=""
	CONFIG_SND_AOA_ONYX=""
	CONFIG_I2C_POWERMAC=""
	CONFIG_SND_AOA_TAS=""
	CONFIG_SND_AOA_TOONIE=""
	CONFIG_SND_AOA_SOUNDBUS=""
	CONFIG_SND_AOA_SOUNDBUS_I2S=""
	CONFIG_ARM=""
	CONFIG_SND_SA11XX_UDA1341=""
	CONFIG_ARCH_SA1100=""
	CONFIG_L3=""
	CONFIG_SND_ARMAACI=""
	CONFIG_ARM_AMBA=""
	CONFIG_SND_PXA2XX_PCM=""
	CONFIG_SND_PXA2XX_AC97=""
	CONFIG_ARCH_PXA=""
	CONFIG_SND_S3C2410=""
	CONFIG_ARCH_S3C2410=""
	CONFIG_I2C_SENSOR=""
	CONFIG_SND_PXA2XX_I2SOUND=""
	CONFIG_SND_AT73C213=""
	CONFIG_ATMEL_SSC=""
	CONFIG_SND_AT73C213_TARGET_BITRATE=""
	CONFIG_MIPS=""
	CONFIG_SND_AU1X00=""
	CONFIG_SOC_AU1000=""
	CONFIG_SOC_AU1100=""
	CONFIG_SOC_AU1500=""
	CONFIG_SUPERH=""
	CONFIG_SND_AICA=""
	CONFIG_SH_DREAMCAST=""
	CONFIG_USB=""
	CONFIG_SND_USB_AUDIO=""
	CONFIG_SND_USB_USX2Y=""
	CONFIG_ALPHA=""
	CONFIG_SND_USB_CAIAQ=""
	CONFIG_SND_USB_CAIAQ_INPUT=""
	CONFIG_PCMCIA=""
	CONFIG_SND_VXPOCKET=""
	CONFIG_SND_PDAUDIOCF=""
	CONFIG_SPARC=""
	CONFIG_SND_SUN_AMD7930=""
	CONFIG_SBUS=""
	CONFIG_SND_SUN_CS4231=""
	CONFIG_SND_SUN_DBRI=""
	CONFIG_GSC=""
	CONFIG_SND_HARMONY=""
	CONFIG_SND_SOC_AC97_BUS=""
	CONFIG_SND_SOC=""
	CONFIG_SND_AT91_SOC=""
	CONFIG_ARCH_AT91=""
	CONFIG_SND_AT91_SOC_SSC=""
	CONFIG_SND_AT91_SOC_ETI_B1_WM8731=""
	CONFIG_MACH_ETI_B1=""
	CONFIG_MACH_ETI_C1=""
	CONFIG_SND_AT91_SOC_ETI_SLAVE=""
	CONFIG_SND_PXA2XX_SOC=""
	CONFIG_SND_PXA2XX_SOC_AC97=""
	CONFIG_SND_PXA2XX_SOC_I2S=""
	CONFIG_SND_PXA2XX_SOC_CORGI=""
	CONFIG_PXA_SHARP_C7XX=""
	CONFIG_SND_PXA2XX_SOC_SPITZ=""
	CONFIG_PXA_SHARP_CXX00=""
	CONFIG_SND_PXA2XX_SOC_POODLE=""
	CONFIG_MACH_POODLE=""
	CONFIG_SND_PXA2XX_SOC_TOSA=""
	CONFIG_MACH_TOSA=""
	CONFIG_SND_PXA2XX_SOC_E800=""
	CONFIG_MACH_E800=""
	CONFIG_SND_S3C24XX_SOC=""
	CONFIG_SND_S3C24XX_SOC_I2S=""
	CONFIG_SND_S3C2412_SOC_I2S=""
	CONFIG_SND_S3C2443_SOC_AC97=""
	CONFIG_SND_S3C24XX_SOC_NEO1973_WM8753=""
	CONFIG_MACH_NEO1973_GTA01=""
	CONFIG_SND_S3C24XX_SOC_SMDK2443_WM9710=""
	CONFIG_MACH_SMDK2443=""
	CONFIG_SND_S3C24XX_SOC_LN2440SBC_ALC650=""
	CONFIG_SND_SOC_PCM_SH7760=""
	CONFIG_CPU_SUBTYPE_SH7760=""
	CONFIG_SH_DMABRG=""
	CONFIG_SND_SOC_SH4_HAC=""
	CONFIG_SND_SOC_SH4_SSI=""
	CONFIG_SND_SH7760_AC97=""
	CONFIG_SND_SOC_MPC8610=""
	CONFIG_MPC8610_HPCD=""
	CONFIG_SND_SOC_MPC8610_HPCD=""
	CONFIG_SND_SOC_AC97_CODEC=""
	CONFIG_SND_SOC_WM8731=""
	CONFIG_SND_SOC_WM8750=""
	CONFIG_SND_SOC_WM8753=""
	CONFIG_SND_SOC_WM9712=""
	CONFIG_SND_SOC_CS4270=""
	CONFIG_SND_SOC_CS4270_HWMUTE=""
	CONFIG_SND_SOC_CS4270_VD33_ERRATA=""
	CONFIG_SND_SOC_TLV320AIC3X=""
	CONFIG_SOUND_PRIME=""
	CONFIG_AC97_BUS=""
])

AC_DEFUN([ALSA_TOPLEVEL_SELECT], [
dnl Check for which cards to compile driver for...
AC_MSG_CHECKING(for which soundcards to compile driver for)
AC_ARG_WITH(cards,
  [  --with-cards=<list>     compile driver for cards in <list>; ]
  [                        cards may be separated with commas; ]
  [                        'all' compiles all drivers; ]
  [                        Possible cards are: ]
  [                          seq-dummy, dummy, virmidi, mtpav, mts64, ]
  [                          serial-u16550, mpu401, portman2x4, ml403-ac97cr, ]
  [                          serialmidi, loopback, pcsp, adlib, ad1816a, ]
  [                          ad1848, als100, azt2320, cmi8330, cs4231, cs4232, ]
  [                          cs4236, dt019x, es968, es1688, es18xx, sc6000, ]
  [                          gusclassic, gusextreme, gusmax, interwave, ]
  [                          interwave-stb, opl3sa2, opti92x-ad1848, ]
  [                          opti92x-cs4231, opti93x, miro, sb8, sb16, sbawe, ]
  [                          sgalaxy, sscape, wavefront, pc98-cs4232, ]
  [                          msnd-pinnacle, ad1889, als300, als4000, ali5451, ]
  [                          atiixp, atiixp-modem, au8810, au8820, au8830, ]
  [                          azt3328, bt87x, ca0106, cmipci, oxygen, cs4281, ]
  [                          cs46xx, cs5530, cs5535audio, darla20, gina20, ]
  [                          layla20, darla24, gina24, layla24, mona, mia, ]
  [                          echo3g, indigo, indigoio, indigodj, emu10k1, ]
  [                          emu10k1x, ens1370, ens1371, es1938, es1968, ]
  [                          fm801, fm801-tea575x, hda-intel, hdsp, hdspm, ]
  [                          hifier, ice1712, ice1724, intel8x0, intel8x0m, ]
  [                          korg1212, maestro3, mixart, nm256, pcxhr, ]
  [                          riptide, rme32, rme96, rme9652, sis7019, ]
  [                          sonicvibes, trident, via82xx, via82xx-modem, ]
  [                          virtuoso, vx222, ymfpci, pdplus, asihpi, ]
  [                          powermac, ps3, aoa, aoa-fabric-layout, aoa-onyx, ]
  [                          aoa-tas, aoa-toonie, aoa-soundbus, ]
  [                          aoa-soundbus-i2s, sa11xx-uda1341, armaaci, ]
  [                          s3c2410, pxa2xx-i2sound, at73c213, au1x00, aica, ]
  [                          usb-audio, usb-usx2y, usb-caiaq, vxpocket, ]
  [                          pdaudiocf, sun-amd7930, sun-cs4231, sun-dbri, ]
  [                          harmony, soc, at91-soc, at91-soc-eti-b1-wm8731, ]
  [                          pxa2xx-soc, pxa2xx-soc-corgi, pxa2xx-soc-spitz, ]
  [                          pxa2xx-soc-poodle, pxa2xx-soc-tosa, ]
  [                          pxa2xx-soc-e800, s3c24xx-soc, soc-pcm-sh7760, ]
  [                          sh7760-ac97 ],
  cards="$withval", cards="all")
SELECTED_CARDS=`echo $cards | sed 's/,/ /g'`
for card in $SELECTED_CARDS; do
  probed=
  if test "$card" = "all" -o "$card" = "seq-dummy"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_SEQUENCER" = "y" -o "$CONFIG_SND_SEQUENCER" = "m" ); then
      CONFIG_SND_SEQ_DUMMY="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "dummy"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_DUMMY="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "virmidi"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_SEQUENCER" = "y" -o "$CONFIG_SND_SEQUENCER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_VIRMIDI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "mtpav"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_MTPAV="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "mts64"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PARPORT" = "y" -o "$CONFIG_PARPORT" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 10 ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_MTS64="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "serial-u16550"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_SERIAL_U16550="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "mpu401"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_MPU401="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "portman2x4"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PARPORT" = "y" -o "$CONFIG_PARPORT" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 10 ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PORTMAN2X4="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ml403-ac97cr"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_XILINX_VIRTEX" = "y" -o "$CONFIG_XILINX_VIRTEX" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ML403_AC97CR="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "serialmidi"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_BROKEN" = "y" -o "$CONFIG_BROKEN" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_SERIALMIDI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "loopback"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_LOOPBACK="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pcsp"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_EXPERIMENTAL" = "y" -o "$CONFIG_EXPERIMENTAL" = "m" ) &&
      ( test "$CONFIG_X86_PC" = "y" -o "$CONFIG_X86_PC" = "m" ) &&
      ( test "$CONFIG_HIGH_RES_TIMERS" = "y" -o "$CONFIG_HIGH_RES_TIMERS" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 24 ); then
      CONFIG_SND_PCSP="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "adlib"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_ADLIB="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ad1816a"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_ISAPNP" = "y" -o "$CONFIG_ISAPNP" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AD1816A="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ad1848"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AD1848_LIB="m"
      CONFIG_SND_AD1848="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "als100"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_ISAPNP" = "y" -o "$CONFIG_ISAPNP" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_SB16_DSP="m"
      CONFIG_SND_ALS100="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "azt2320"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_ISAPNP" = "y" -o "$CONFIG_ISAPNP" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_AZT2320="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cmi8330"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_AD1848_LIB="m"
      CONFIG_SND_SB16_DSP="m"
      CONFIG_SND_CMI8330="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs4231"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_CS4231="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs4232"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_CS4232="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs4236"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_CS4236="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "dt019x"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_ISAPNP" = "y" -o "$CONFIG_ISAPNP" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_SB16_DSP="m"
      CONFIG_SND_DT019X="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "es968"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_ISAPNP" = "y" -o "$CONFIG_ISAPNP" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_SB8_DSP="m"
      CONFIG_SND_ES968="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "es1688"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_ES1688="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "es18xx"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_ES18XX="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sc6000"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_HAS_IOPORT" = "y" -o "$CONFIG_HAS_IOPORT" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_AD1848_LIB="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_SC6000="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "gusclassic"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_GUS_SYNTH="m"
      CONFIG_SND_GUSCLASSIC="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "gusextreme"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_GUS_SYNTH="m"
      CONFIG_SND_GUSEXTREME="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "gusmax"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_GUS_SYNTH="m"
      CONFIG_SND_GUSMAX="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "interwave"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_GUS_SYNTH="m"
      CONFIG_SND_INTERWAVE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "interwave-stb"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_PNP" = "y" -o "$CONFIG_PNP" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_GUS_SYNTH="m"
      CONFIG_SND_INTERWAVE_STB="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "opl3sa2"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_OPL3SA2="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "opti92x-ad1848"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_OPL4_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AD1848_LIB="m"
      CONFIG_SND_OPTI92X_AD1848="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "opti92x-cs4231"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_OPL4_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_OPTI92X_CS4231="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "opti93x"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPTI93X="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "miro"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL4_LIB="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_MIRO="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sb8"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_SB8_DSP="m"
      CONFIG_SND_SB8="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sb16"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_SB16_DSP="m"
      CONFIG_SND_SB16="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sbawe"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_SB16_DSP="m"
      CONFIG_SND_SBAWE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sgalaxy"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AD1848_LIB="m"
      CONFIG_SND_SGALAXY="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sscape"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_SSCAPE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "wavefront"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_CS4231_LIB="m"
      CONFIG_SND_WAVEFRONT="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pc98-cs4232"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_X86_PC9800" = "y" -o "$CONFIG_X86_PC9800" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_PC98_CS4232="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "msnd-pinnacle"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_X86" = "y" -o "$CONFIG_X86" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_MSND_PINNACLE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ad1889"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_AD1889="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "als300"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_ALS300="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "als4000"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_ALS4000="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ali5451"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ALI5451="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "atiixp"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ATIIXP="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "atiixp-modem"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ATIIXP_MODEM="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "au8810"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_AU8810="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "au8820"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_AU8820="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "au8830"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_AU8830="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "azt3328"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_EXPERIMENTAL" = "y" -o "$CONFIG_EXPERIMENTAL" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AZT3328="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "bt87x"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_BT87X="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ca0106"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_CA0106="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cmipci"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_CMIPCI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "oxygen"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_OXYGEN_LIB="m"
      CONFIG_SND_OXYGEN="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs4281"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_CS4281="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs46xx"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_CS46XX="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs5530"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SB_COMMON="m"
      CONFIG_SND_SB16_DSP="m"
      CONFIG_SND_CS5530="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs5535audio"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_X86" = "y" -o "$CONFIG_X86" = "m" ) &&
       ! ( test "$CONFIG_X86_64" = "y" -o "$CONFIG_X86_64" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 10 ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_CS5535AUDIO="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "darla20"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_DARLA20="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "gina20"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_GINA20="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "layla20"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_LAYLA20="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "darla24"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_DARLA24="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "gina24"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_GINA24="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "layla24"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_LAYLA24="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "mona"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_MONA="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "mia"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_MIA="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "echo3g"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_ECHO3G="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "indigo"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_INDIGO="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "indigoio"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_INDIGOIO="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "indigodj"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_INDIGODJ="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "emu10k1"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_EMU10K1="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "emu10k1x"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_EMU10K1X="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ens1370"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_ENS1370="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ens1371"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ENS1371="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "es1938"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ES1938="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "es1968"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ES1968="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "fm801"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_FM801="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "fm801-tea575x"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_FM801_TEA575X_BOOL" = "y" -o "$CONFIG_SND_FM801_TEA575X_BOOL" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_VIDEO_V4L1" = "y" -o "$CONFIG_VIDEO_V4L1" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_VIDEO_DEV" = "y" -o "$CONFIG_VIDEO_DEV" = "m" ); then
      CONFIG_SND_FM801_TEA575X="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-intel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HDA_INTEL="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hdsp"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HDSP="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hdspm"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HDSPM="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hifier"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_OXYGEN_LIB="m"
      CONFIG_SND_HIFIER="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ice1712"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ICE1712="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ice1724"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ICE1724="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "intel8x0"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_INTEL8X0="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "intel8x0m"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_INTEL8X0M="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "korg1212"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_KORG1212="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "maestro3"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_MAESTRO3="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "mixart"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_MIXART="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "nm256"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_NM256="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pcxhr"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_PCXHR="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "riptide"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_RIPTIDE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "rme32"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RME32="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "rme96"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RME96="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "rme9652"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RME9652="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sis7019"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_X86" = "y" -o "$CONFIG_X86" = "m" ) &&
       ! ( test "$CONFIG_X86_64" = "y" -o "$CONFIG_X86_64" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_SIS7019="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sonicvibes"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_SONICVIBES="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "trident"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_TRIDENT="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "via82xx"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_VIA82XX="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "via82xx-modem"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_VIA82XX_MODEM="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "virtuoso"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_OXYGEN_LIB="m"
      CONFIG_SND_VIRTUOSO="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "vx222"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_VX_LIB="m"
      CONFIG_SND_VX222="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ymfpci"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_OPL3_LIB="m"
      CONFIG_SND_MPU401_UART="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_YMFPCI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pdplus"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_PDPLUS="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "asihpi"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_X86" = "y" -o "$CONFIG_X86" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 10 ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_ASIHPI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "powermac"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC" = "y" -o "$CONFIG_PPC" = "m" ) &&
      ( test "$CONFIG_I2C" = "y" -o "$CONFIG_I2C" = "m" ) &&
      ( test "$CONFIG_INPUT" = "y" -o "$CONFIG_INPUT" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_POWERMAC="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ps3"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( ( test "$CONFIG_PPC64" = "y" -o "$CONFIG_PPC64" = "m" ) ||
      ( test "$CONFIG_PPC32" = "y" -o "$CONFIG_PPC32" = "m" ) ) ) &&
      ( test "$CONFIG_PS3_PS3AV" = "y" -o "$CONFIG_PS3_PS3AV" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_PS3="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 16 ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AOA="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa-fabric-layout"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_SND_AOA" = "y" -o "$CONFIG_SND_AOA" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AOA_SOUNDBUS="m"
      CONFIG_SND_AOA_SOUNDBUS_I2S="m"
      CONFIG_SND_AOA_FABRIC_LAYOUT="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa-onyx"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_SND_AOA" = "y" -o "$CONFIG_SND_AOA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_I2C" = "y" -o "$CONFIG_I2C" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_I2C_POWERMAC" = "y" -o "$CONFIG_I2C_POWERMAC" = "m" ); then
      CONFIG_SND_AOA_ONYX="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa-tas"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_SND_AOA" = "y" -o "$CONFIG_SND_AOA" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_I2C" = "y" -o "$CONFIG_I2C" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_I2C_POWERMAC" = "y" -o "$CONFIG_I2C_POWERMAC" = "m" ); then
      CONFIG_SND_AOA_TAS="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa-toonie"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_SND_AOA" = "y" -o "$CONFIG_SND_AOA" = "m" ); then
      CONFIG_SND_AOA_TOONIE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa-soundbus"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AOA_SOUNDBUS="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aoa-soundbus-i2s"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC_PMAC" = "y" -o "$CONFIG_PPC_PMAC" = "m" ) &&
      ( test "$CONFIG_SND_AOA_SOUNDBUS" = "y" -o "$CONFIG_SND_AOA_SOUNDBUS" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ); then
      CONFIG_SND_AOA_SOUNDBUS_I2S="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sa11xx-uda1341"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARM" = "y" -o "$CONFIG_ARM" = "m" ) &&
      ( test "$CONFIG_ARCH_SA1100" = "y" -o "$CONFIG_ARCH_SA1100" = "m" ) &&
      ( test "$CONFIG_L3" = "y" -o "$CONFIG_L3" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SA11XX_UDA1341="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "armaaci"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARM" = "y" -o "$CONFIG_ARM" = "m" ) &&
      ( test "$CONFIG_ARM_AMBA" = "y" -o "$CONFIG_ARM_AMBA" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_ARMAACI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "s3c2410"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARM" = "y" -o "$CONFIG_ARM" = "m" ) &&
      ( test "$CONFIG_ARCH_S3C2410" = "y" -o "$CONFIG_ARCH_S3C2410" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARM" = "y" -o "$CONFIG_ARM" = "m" ) &&
      ( test "$CONFIG_I2C_SENSOR" = "y" -o "$CONFIG_I2C_SENSOR" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_S3C2410="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-i2sound"; then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_PXA2XX_I2SOUND="m"
      probed=1
  fi
  if test "$card" = "all" -o "$card" = "at73c213"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ATMEL_SSC" = "y" -o "$CONFIG_ATMEL_SSC" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AT73C213="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "au1x00"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_MIPS" = "y" -o "$CONFIG_MIPS" = "m" ) &&
      ( ( test "$CONFIG_SOC_AU1000" = "y" -o "$CONFIG_SOC_AU1000" = "m" ) ||
      ( test "$CONFIG_SOC_AU1100" = "y" -o "$CONFIG_SOC_AU1100" = "m" ) ||
      ( test "$CONFIG_SOC_AU1500" = "y" -o "$CONFIG_SOC_AU1500" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_AU1X00="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "aica"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SUPERH" = "y" -o "$CONFIG_SUPERH" = "m" ) &&
      ( test "$CONFIG_SH_DREAMCAST" = "y" -o "$CONFIG_SH_DREAMCAST" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AICA="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "usb-audio"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_USB" = "y" -o "$CONFIG_USB" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_USB_AUDIO="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "usb-usx2y"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_USB" = "y" -o "$CONFIG_USB" = "m" ) &&
      ( ( test "$CONFIG_X86" = "y" -o "$CONFIG_X86" = "m" ) ||
      ( test "$CONFIG_PPC" = "y" -o "$CONFIG_PPC" = "m" ) ||
      ( test "$CONFIG_ALPHA" = "y" -o "$CONFIG_ALPHA" = "m" ) ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_USB_USX2Y="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "usb-caiaq"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_USB" = "y" -o "$CONFIG_USB" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 15 ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_RAWMIDI="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_USB_CAIAQ="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "vxpocket"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCMCIA" = "y" -o "$CONFIG_PCMCIA" = "m" ); then
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_VX_LIB="m"
      CONFIG_SND_VXPOCKET="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pdaudiocf"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCMCIA" = "y" -o "$CONFIG_PCMCIA" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_PDAUDIOCF="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sun-amd7930"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SPARC" = "y" -o "$CONFIG_SPARC" = "m" ) &&
      ( test "$CONFIG_SBUS" = "y" -o "$CONFIG_SBUS" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SUN_AMD7930="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sun-cs4231"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SPARC" = "y" -o "$CONFIG_SPARC" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SUN_CS4231="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sun-dbri"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SPARC" = "y" -o "$CONFIG_SPARC" = "m" ) &&
      ( test "$CONFIG_SBUS" = "y" -o "$CONFIG_SBUS" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SUN_DBRI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "harmony"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_GSC" = "y" -o "$CONFIG_GSC" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_HARMONY="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 10 ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_SOC="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "at91-soc"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARCH_AT91" = "y" -o "$CONFIG_ARCH_AT91" = "m" ) &&
      ( test "$CONFIG_SND_SOC" = "y" -o "$CONFIG_SND_SOC" = "m" ); then
      CONFIG_SND_AT91_SOC="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "at91-soc-eti-b1-wm8731"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_AT91_SOC" = "y" -o "$CONFIG_SND_AT91_SOC" = "m" ) &&
      ( ( test "$CONFIG_MACH_ETI_B1" = "y" -o "$CONFIG_MACH_ETI_B1" = "m" ) ||
      ( test "$CONFIG_MACH_ETI_C1" = "y" -o "$CONFIG_MACH_ETI_C1" = "m" ) ); then
      CONFIG_SND_AT91_SOC_SSC="m"
      CONFIG_SND_SOC_WM8731="m"
      CONFIG_SND_AT91_SOC_ETI_B1_WM8731="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-soc"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARCH_PXA" = "y" -o "$CONFIG_ARCH_PXA" = "m" ) &&
      ( test "$CONFIG_SND_SOC" = "y" -o "$CONFIG_SND_SOC" = "m" ); then
      CONFIG_SND_PXA2XX_SOC="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-soc-corgi"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_PXA2XX_SOC" = "y" -o "$CONFIG_SND_PXA2XX_SOC" = "m" ) &&
      ( test "$CONFIG_PXA_SHARP_C7xx" = "y" -o "$CONFIG_PXA_SHARP_C7xx" = "m" ); then
      CONFIG_SND_PXA2XX_SOC_I2S="m"
      CONFIG_SND_SOC_WM8731="m"
      CONFIG_SND_PXA2XX_SOC_CORGI="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-soc-spitz"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_PXA2XX_SOC" = "y" -o "$CONFIG_SND_PXA2XX_SOC" = "m" ) &&
      ( test "$CONFIG_PXA_SHARP_Cxx00" = "y" -o "$CONFIG_PXA_SHARP_Cxx00" = "m" ); then
      CONFIG_SND_PXA2XX_SOC_I2S="m"
      CONFIG_SND_SOC_WM8750="m"
      CONFIG_SND_PXA2XX_SOC_SPITZ="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-soc-poodle"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_PXA2XX_SOC" = "y" -o "$CONFIG_SND_PXA2XX_SOC" = "m" ) &&
      ( test "$CONFIG_MACH_POODLE" = "y" -o "$CONFIG_MACH_POODLE" = "m" ); then
      CONFIG_SND_PXA2XX_SOC_I2S="m"
      CONFIG_SND_SOC_WM8731="m"
      CONFIG_SND_PXA2XX_SOC_POODLE="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-soc-tosa"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_PXA2XX_SOC" = "y" -o "$CONFIG_SND_PXA2XX_SOC" = "m" ) &&
      ( test "$CONFIG_MACH_TOSA" = "y" -o "$CONFIG_MACH_TOSA" = "m" ); then
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_SOC_AC97_BUS="y"
      CONFIG_SND_PXA2XX_SOC_AC97="m"
      CONFIG_SND_SOC_WM9712="m"
      CONFIG_SND_PXA2XX_SOC_TOSA="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pxa2xx-soc-e800"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_PXA2XX_SOC" = "y" -o "$CONFIG_SND_PXA2XX_SOC" = "m" ) &&
      ( test "$CONFIG_MACH_E800" = "y" -o "$CONFIG_MACH_E800" = "m" ); then
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_SOC_AC97_BUS="y"
      CONFIG_SND_SOC_WM9712="m"
      CONFIG_SND_PXA2XX_SOC_AC97="m"
      CONFIG_SND_PXA2XX_SOC_E800="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "s3c24xx-soc"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_ARCH_S3C2410" = "y" -o "$CONFIG_ARCH_S3C2410" = "m" ) &&
      ( test "$CONFIG_SND_SOC" = "y" -o "$CONFIG_SND_SOC" = "m" ); then
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_S3C24XX_SOC="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc-pcm-sh7760"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SUPERH" = "y" -o "$CONFIG_SUPERH" = "m" ) &&
      ( test "$CONFIG_CPU_SUBTYPE_SH7760" = "y" -o "$CONFIG_CPU_SUBTYPE_SH7760" = "m" ) &&
      ( test "$CONFIG_SND_SOC" = "y" -o "$CONFIG_SND_SOC" = "m" ) &&
      ( test "$CONFIG_SH_DMABRG" = "y" -o "$CONFIG_SH_DMABRG" = "m" ); then
      CONFIG_SND_SOC_PCM_SH7760="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sh7760-ac97"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SUPERH" = "y" -o "$CONFIG_SUPERH" = "m" ) &&
      ( test "$CONFIG_CPU_SUBTYPE_SH7760" = "y" -o "$CONFIG_CPU_SUBTYPE_SH7760" = "m" ) &&
      ( test "$CONFIG_SND_SOC_PCM_SH7760" = "y" -o "$CONFIG_SND_SOC_PCM_SH7760" = "m" ); then
      test "$kversion.$kpatchlevel" = "2.6" -a $ksublevel -ge 14 && CONFIG_AC97_BUS="m"
      CONFIG_SND_SOC_AC97_BUS="y"
      CONFIG_SND_TIMER="m"
      CONFIG_SND_PCM="m"
      CONFIG_SND_AC97_CODEC="m"
      CONFIG_SND_SOC_SH4_HAC="m"
      CONFIG_SND_SOC_AC97_CODEC="m"
      CONFIG_SND_SH7760_AC97="m"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test -z "$probed"; then
    AC_MSG_ERROR(Unknown soundcard $card)
  elif test "$probed" = "0"; then
    AC_MSG_ERROR(Unsupported soundcard $card)
  fi
done

AC_ARG_WITH(card_options,
  [  --with-card-options=<list> enable driver options in <list>; ]
  [                        options may be separated with commas; ]
  [                        'all' enables all options; ]
  [                        Possible options are: ]
  [                          seq-rtctimer-default, support-old-api, ]
  [                          pcm-xrun-debug, sb16-csp, ]
  [                          sb16-csp-firmware-in-kernel, ]
  [                          wavefront-firmware-in-kernel, bt87x-overclock, ]
  [                          cs46xx-new-dsp, fm801-tea575x-bool, hda-hwdep, ]
  [                          hda-codec-realtek, hda-codec-analog, ]
  [                          hda-codec-sigmatel, hda-codec-via, ]
  [                          hda-codec-atihdmi, hda-codec-conexant, ]
  [                          hda-codec-cmedia, hda-codec-si3054, hda-generic, ]
  [                          hda-power-save, korg1212-firmware-in-kernel, ]
  [                          maestro3-firmware-in-kernel, ]
  [                          ymfpci-firmware-in-kernel, ac97-power-save, ]
  [                          powermac-auto-drc, usb-caiaq-input, soc-ac97-bus, ]
  [                          at91-soc-eti-slave, soc-mpc8610, ]
  [                          soc-mpc8610-hpcd, soc-cs4270-hwmute, ]
  [                          soc-cs4270-vd33-errata ],
  cards="$withval", cards="all")
SELECTED_OPTIONS=`echo $cards | sed 's/,/ /g'`
for card in $SELECTED_OPTIONS; do
  probed=
  if test "$card" = "all" -o "$card" = "seq-rtctimer-default"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_RTCTIMER" = "y" -o "$CONFIG_SND_RTCTIMER" = "m" ) &&
      ( test "$CONFIG_SND_SEQUENCER" = "y" -o "$CONFIG_SND_SEQUENCER" = "m" ); then
      CONFIG_SND_SEQ_RTCTIMER_DEFAULT="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "support-old-api"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ); then
      CONFIG_SND_SUPPORT_OLD_API="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "pcm-xrun-debug"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_DEBUG" = "y" -o "$CONFIG_SND_DEBUG" = "m" ) &&
      ( test "$CONFIG_SND_VERBOSE_PROCFS" = "y" -o "$CONFIG_SND_VERBOSE_PROCFS" = "m" ); then
      CONFIG_SND_PCM_XRUN_DEBUG="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sb16-csp"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( ( test "$CONFIG_SND_SB16" = "y" -o "$CONFIG_SND_SB16" = "m" ) ||
      ( test "$CONFIG_SND_SBAWE" = "y" -o "$CONFIG_SND_SBAWE" = "m" ) ) &&
      ( ( test "$CONFIG_BROKEN" = "y" -o "$CONFIG_BROKEN" = "m" ) ||
       ! ( test "$CONFIG_PPC" = "y" -o "$CONFIG_PPC" = "m" ) ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_FW_LOADER" = "y" -o "$CONFIG_FW_LOADER" = "m" ); then
      CONFIG_SND_SB16_CSP="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "sb16-csp-firmware-in-kernel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_SND_SB16_CSP" = "y" -o "$CONFIG_SND_SB16_CSP" = "m" ); then
      CONFIG_SND_SB16_CSP_FIRMWARE_IN_KERNEL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "wavefront-firmware-in-kernel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( test "$CONFIG_ISA" = "y" -o "$CONFIG_ISA" = "m" ) &&
      ( test "$CONFIG_ISA_DMA_API" = "y" -o "$CONFIG_ISA_DMA_API" = "m" ) ) &&
      ( test "$CONFIG_SND_WAVEFRONT" = "y" -o "$CONFIG_SND_WAVEFRONT" = "m" ); then
      CONFIG_SND_WAVEFRONT_FIRMWARE_IN_KERNEL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "bt87x-overclock"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_BT87X" = "y" -o "$CONFIG_SND_BT87X" = "m" ); then
      CONFIG_SND_BT87X_OVERCLOCK="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "cs46xx-new-dsp"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_CS46XX" = "y" -o "$CONFIG_SND_CS46XX" = "m" ); then
      CONFIG_SND_CS46XX_NEW_DSP="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "fm801-tea575x-bool"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_FM801" = "y" -o "$CONFIG_SND_FM801" = "m" ); then
      CONFIG_SND_FM801_TEA575X_BOOL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-hwdep"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HWDEP="m"
      CONFIG_SND_HDA_HWDEP="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-realtek"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_REALTEK="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-analog"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_ANALOG="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-sigmatel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_SIGMATEL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-via"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_VIA="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-atihdmi"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_ATIHDMI="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-conexant"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_CONEXANT="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-cmedia"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_CMEDIA="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-codec-si3054"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_CODEC_SI3054="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-generic"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ); then
      CONFIG_SND_HDA_GENERIC="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-power-save"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_INTEL" = "y" -o "$CONFIG_SND_HDA_INTEL" = "m" ) &&
      ( test "$CONFIG_EXPERIMENTAL" = "y" -o "$CONFIG_EXPERIMENTAL" = "m" ); then
      CONFIG_SND_HDA_POWER_SAVE="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "hda-power-save-default"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_HDA_POWER_SAVE" = "y" -o "$CONFIG_SND_HDA_POWER_SAVE" = "m" ); then
      CONFIG_SND_HDA_POWER_SAVE_DEFAULT="0"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "korg1212-firmware-in-kernel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_KORG1212" = "y" -o "$CONFIG_SND_KORG1212" = "m" ); then
      CONFIG_SND_KORG1212_FIRMWARE_IN_KERNEL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "maestro3-firmware-in-kernel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_MAESTRO3" = "y" -o "$CONFIG_SND_MAESTRO3" = "m" ); then
      CONFIG_SND_MAESTRO3_FIRMWARE_IN_KERNEL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ymfpci-firmware-in-kernel"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_YMFPCI" = "y" -o "$CONFIG_SND_YMFPCI" = "m" ); then
      CONFIG_SND_YMFPCI_FIRMWARE_IN_KERNEL="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ac97-power-save"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_AC97_CODEC" = "y" -o "$CONFIG_SND_AC97_CODEC" = "m" ) &&
      ( test "$CONFIG_EXPERIMENTAL" = "y" -o "$CONFIG_EXPERIMENTAL" = "m" ); then
      CONFIG_SND_AC97_POWER_SAVE="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ac97-power-save-default"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PCI" = "y" -o "$CONFIG_PCI" = "m" ) &&
      ( test "$CONFIG_SND_AC97_POWER_SAVE" = "y" -o "$CONFIG_SND_AC97_POWER_SAVE" = "m" ); then
      CONFIG_SND_AC97_POWER_SAVE_DEFAULT="0"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "powermac-auto-drc"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_PPC" = "y" -o "$CONFIG_PPC" = "m" ) &&
      ( test "$CONFIG_SND_POWERMAC" = "y" -o "$CONFIG_SND_POWERMAC" = "m" ); then
      CONFIG_SND_POWERMAC_AUTO_DRC="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "ps3-default-start-delay"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( ( ( test "$CONFIG_PPC64" = "y" -o "$CONFIG_PPC64" = "m" ) ||
      ( test "$CONFIG_PPC32" = "y" -o "$CONFIG_PPC32" = "m" ) ) ) &&
      ( test "$CONFIG_SND_PS3" = "y" -o "$CONFIG_SND_PS3" = "m" ); then
      CONFIG_SND_PS3_DEFAULT_START_DELAY="2000"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "at73c213-target-bitrate"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_AT73C213" = "y" -o "$CONFIG_SND_AT73C213" = "m" ); then
      CONFIG_SND_AT73C213_TARGET_BITRATE="48000"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "usb-caiaq-input"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_USB" = "y" -o "$CONFIG_USB" = "m" ) &&
      ( test "$CONFIG_SND_USB_CAIAQ" = "y" -o "$CONFIG_SND_USB_CAIAQ" = "m" ) &&
      ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_USB" = "y" -o "$CONFIG_USB" = "m" ) &&
      ( test "$CONFIG_INPUT" = "y" -o "$CONFIG_INPUT" = "m" ); then
      CONFIG_SND_USB_CAIAQ_INPUT="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc-ac97-bus"; then
      CONFIG_SND_SOC_AC97_BUS="y"
      probed=1
  fi
  if test "$card" = "all" -o "$card" = "at91-soc-eti-slave"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_AT91_SOC_ETI_B1_WM8731" = "y" -o "$CONFIG_SND_AT91_SOC_ETI_B1_WM8731" = "m" ); then
      CONFIG_SND_AT91_SOC_ETI_SLAVE="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc-mpc8610"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_SOC" = "y" -o "$CONFIG_SND_SOC" = "m" ) &&
      ( test "$CONFIG_MPC8610_HPCD" = "y" -o "$CONFIG_MPC8610_HPCD" = "m" ); then
      CONFIG_SND_SOC_MPC8610="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc-mpc8610-hpcd"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_SOC_MPC8610" = "y" -o "$CONFIG_SND_SOC_MPC8610" = "m" ); then
      CONFIG_SND_SOC_CS4270="m"
      CONFIG_SND_SOC_CS4270_VD33_ERRATA="y"
      CONFIG_SND_SOC_MPC8610_HPCD="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc-cs4270-hwmute"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_SOC_CS4270" = "y" -o "$CONFIG_SND_SOC_CS4270" = "m" ); then
      CONFIG_SND_SOC_CS4270_HWMUTE="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test "$card" = "all" -o "$card" = "soc-cs4270-vd33-errata"; then
    if ( test "$CONFIG_SOUND" = "y" -o "$CONFIG_SOUND" = "m" ) &&
      ( test "$CONFIG_SND_SOC_CS4270" = "y" -o "$CONFIG_SND_SOC_CS4270" = "m" ); then
      CONFIG_SND_SOC_CS4270_VD33_ERRATA="y"
      probed=1
    elif test -z "$probed"; then
      probed=0
    fi
  fi
  if test -z "$probed"; then
    AC_MSG_ERROR(Unknown option $card)
  elif test "$probed" = "0" -a "$card" != "all"; then
    AC_MSG_ERROR(Unsupported option $card)
  fi
done

AC_MSG_RESULT($SELECTED_CARDS)

CONFIG_SND="m"
])

AC_DEFUN([ALSA_TOPLEVEL_DEFINES], [
if test -n "$CONFIG_SOUND"; then
  AC_DEFINE(CONFIG_SOUND_MODULE)
fi
if test -n "$CONFIG_SND_TIMER"; then
  AC_DEFINE(CONFIG_SND_TIMER_MODULE)
fi
if test -n "$CONFIG_SND_PCM"; then
  AC_DEFINE(CONFIG_SND_PCM_MODULE)
fi
if test -n "$CONFIG_SND_HWDEP"; then
  AC_DEFINE(CONFIG_SND_HWDEP_MODULE)
fi
if test -n "$CONFIG_SND_RAWMIDI"; then
  AC_DEFINE(CONFIG_SND_RAWMIDI_MODULE)
fi
if test -n "$CONFIG_SND_SEQ_DUMMY"; then
  AC_DEFINE(CONFIG_SND_SEQ_DUMMY_MODULE)
fi
if test -n "$CONFIG_SND_SEQ_RTCTIMER_DEFAULT"; then
  AC_DEFINE(CONFIG_SND_SEQ_RTCTIMER_DEFAULT)
fi
if test -n "$CONFIG_SND_SUPPORT_OLD_API"; then
  AC_DEFINE(CONFIG_SND_SUPPORT_OLD_API)
fi
if test -n "$CONFIG_PROC_FS"; then
  AC_DEFINE(CONFIG_PROC_FS_MODULE)
fi
if test -n "$CONFIG_SND_PCM_XRUN_DEBUG"; then
  AC_DEFINE(CONFIG_SND_PCM_XRUN_DEBUG)
fi
if test -n "$CONFIG_HPET"; then
  AC_DEFINE(CONFIG_HPET_MODULE)
fi
if test -n "$CONFIG_SND_MPU401_UART"; then
  AC_DEFINE(CONFIG_SND_MPU401_UART_MODULE)
fi
if test -n "$CONFIG_SND_OPL3_LIB"; then
  AC_DEFINE(CONFIG_SND_OPL3_LIB_MODULE)
fi
if test -n "$CONFIG_SND_OPL4_LIB"; then
  AC_DEFINE(CONFIG_SND_OPL4_LIB_MODULE)
fi
if test -n "$CONFIG_SND_VX_LIB"; then
  AC_DEFINE(CONFIG_SND_VX_LIB_MODULE)
fi
if test -n "$CONFIG_SND_AC97_CODEC"; then
  AC_DEFINE(CONFIG_SND_AC97_CODEC_MODULE)
fi
if test -n "$CONFIG_SND_DUMMY"; then
  AC_DEFINE(CONFIG_SND_DUMMY_MODULE)
fi
if test -n "$CONFIG_SND_VIRMIDI"; then
  AC_DEFINE(CONFIG_SND_VIRMIDI_MODULE)
fi
if test -n "$CONFIG_SND_MTPAV"; then
  AC_DEFINE(CONFIG_SND_MTPAV_MODULE)
fi
if test -n "$CONFIG_SND_MTS64"; then
  AC_DEFINE(CONFIG_SND_MTS64_MODULE)
fi
if test -n "$CONFIG_PARPORT"; then
  AC_DEFINE(CONFIG_PARPORT_MODULE)
fi
if test -n "$CONFIG_SND_SERIAL_U16550"; then
  AC_DEFINE(CONFIG_SND_SERIAL_U16550_MODULE)
fi
if test -n "$CONFIG_SND_MPU401"; then
  AC_DEFINE(CONFIG_SND_MPU401_MODULE)
fi
if test -n "$CONFIG_SND_PORTMAN2X4"; then
  AC_DEFINE(CONFIG_SND_PORTMAN2X4_MODULE)
fi
if test -n "$CONFIG_SND_ML403_AC97CR"; then
  AC_DEFINE(CONFIG_SND_ML403_AC97CR_MODULE)
fi
if test -n "$CONFIG_XILINX_VIRTEX"; then
  AC_DEFINE(CONFIG_XILINX_VIRTEX_MODULE)
fi
if test -n "$CONFIG_SND_SERIALMIDI"; then
  AC_DEFINE(CONFIG_SND_SERIALMIDI_MODULE)
fi
if test -n "$CONFIG_BROKEN"; then
  AC_DEFINE(CONFIG_BROKEN_MODULE)
fi
if test -n "$CONFIG_SND_LOOPBACK"; then
  AC_DEFINE(CONFIG_SND_LOOPBACK_MODULE)
fi
if test -n "$CONFIG_SND_PCSP"; then
  AC_DEFINE(CONFIG_SND_PCSP_MODULE)
fi
if test -n "$CONFIG_X86_PC"; then
  AC_DEFINE(CONFIG_X86_PC_MODULE)
fi
if test -n "$CONFIG_HIGH_RES_TIMERS"; then
  AC_DEFINE(CONFIG_HIGH_RES_TIMERS_MODULE)
fi
if test -n "$CONFIG_SND_AD1848_LIB"; then
  AC_DEFINE(CONFIG_SND_AD1848_LIB_MODULE)
fi
if test -n "$CONFIG_SND_CS4231_LIB"; then
  AC_DEFINE(CONFIG_SND_CS4231_LIB_MODULE)
fi
if test -n "$CONFIG_SND_SB_COMMON"; then
  AC_DEFINE(CONFIG_SND_SB_COMMON_MODULE)
fi
if test -n "$CONFIG_SND_SB8_DSP"; then
  AC_DEFINE(CONFIG_SND_SB8_DSP_MODULE)
fi
if test -n "$CONFIG_SND_SB16_DSP"; then
  AC_DEFINE(CONFIG_SND_SB16_DSP_MODULE)
fi
if test -n "$CONFIG_SND_ADLIB"; then
  AC_DEFINE(CONFIG_SND_ADLIB_MODULE)
fi
if test -n "$CONFIG_SND_AD1816A"; then
  AC_DEFINE(CONFIG_SND_AD1816A_MODULE)
fi
if test -n "$CONFIG_PNP"; then
  AC_DEFINE(CONFIG_PNP_MODULE)
fi
if test -n "$CONFIG_SND_AD1848"; then
  AC_DEFINE(CONFIG_SND_AD1848_MODULE)
fi
if test -n "$CONFIG_SND_ALS100"; then
  AC_DEFINE(CONFIG_SND_ALS100_MODULE)
fi
if test -n "$CONFIG_SND_AZT2320"; then
  AC_DEFINE(CONFIG_SND_AZT2320_MODULE)
fi
if test -n "$CONFIG_SND_CMI8330"; then
  AC_DEFINE(CONFIG_SND_CMI8330_MODULE)
fi
if test -n "$CONFIG_SND_CS4231"; then
  AC_DEFINE(CONFIG_SND_CS4231_MODULE)
fi
if test -n "$CONFIG_SND_CS4232"; then
  AC_DEFINE(CONFIG_SND_CS4232_MODULE)
fi
if test -n "$CONFIG_SND_CS4236"; then
  AC_DEFINE(CONFIG_SND_CS4236_MODULE)
fi
if test -n "$CONFIG_SND_DT019X"; then
  AC_DEFINE(CONFIG_SND_DT019X_MODULE)
fi
if test -n "$CONFIG_SND_ES968"; then
  AC_DEFINE(CONFIG_SND_ES968_MODULE)
fi
if test -n "$CONFIG_SND_ES1688"; then
  AC_DEFINE(CONFIG_SND_ES1688_MODULE)
fi
if test -n "$CONFIG_SND_ES18XX"; then
  AC_DEFINE(CONFIG_SND_ES18XX_MODULE)
fi
if test -n "$CONFIG_SND_SC6000"; then
  AC_DEFINE(CONFIG_SND_SC6000_MODULE)
fi
if test -n "$CONFIG_SND_GUS_SYNTH"; then
  AC_DEFINE(CONFIG_SND_GUS_SYNTH_MODULE)
fi
if test -n "$CONFIG_SND_GUSCLASSIC"; then
  AC_DEFINE(CONFIG_SND_GUSCLASSIC_MODULE)
fi
if test -n "$CONFIG_SND_GUSEXTREME"; then
  AC_DEFINE(CONFIG_SND_GUSEXTREME_MODULE)
fi
if test -n "$CONFIG_SND_GUSMAX"; then
  AC_DEFINE(CONFIG_SND_GUSMAX_MODULE)
fi
if test -n "$CONFIG_SND_INTERWAVE"; then
  AC_DEFINE(CONFIG_SND_INTERWAVE_MODULE)
fi
if test -n "$CONFIG_SND_INTERWAVE_STB"; then
  AC_DEFINE(CONFIG_SND_INTERWAVE_STB_MODULE)
fi
if test -n "$CONFIG_SND_OPL3SA2"; then
  AC_DEFINE(CONFIG_SND_OPL3SA2_MODULE)
fi
if test -n "$CONFIG_SND_OPTI92X_AD1848"; then
  AC_DEFINE(CONFIG_SND_OPTI92X_AD1848_MODULE)
fi
if test -n "$CONFIG_SND_OPTI92X_CS4231"; then
  AC_DEFINE(CONFIG_SND_OPTI92X_CS4231_MODULE)
fi
if test -n "$CONFIG_SND_OPTI93X"; then
  AC_DEFINE(CONFIG_SND_OPTI93X_MODULE)
fi
if test -n "$CONFIG_SND_MIRO"; then
  AC_DEFINE(CONFIG_SND_MIRO_MODULE)
fi
if test -n "$CONFIG_SND_SB8"; then
  AC_DEFINE(CONFIG_SND_SB8_MODULE)
fi
if test -n "$CONFIG_SND_SB16"; then
  AC_DEFINE(CONFIG_SND_SB16_MODULE)
fi
if test -n "$CONFIG_SND_SBAWE"; then
  AC_DEFINE(CONFIG_SND_SBAWE_MODULE)
fi
if test -n "$CONFIG_SND_SB16_CSP"; then
  AC_DEFINE(CONFIG_SND_SB16_CSP)
fi
if test -n "$CONFIG_SND_SB16_CSP_FIRMWARE_IN_KERNEL"; then
  AC_DEFINE(CONFIG_SND_SB16_CSP_FIRMWARE_IN_KERNEL)
fi
if test -n "$CONFIG_SND_SGALAXY"; then
  AC_DEFINE(CONFIG_SND_SGALAXY_MODULE)
fi
if test -n "$CONFIG_SND_SSCAPE"; then
  AC_DEFINE(CONFIG_SND_SSCAPE_MODULE)
fi
if test -n "$CONFIG_SND_WAVEFRONT"; then
  AC_DEFINE(CONFIG_SND_WAVEFRONT_MODULE)
fi
if test -n "$CONFIG_SND_WAVEFRONT_FIRMWARE_IN_KERNEL"; then
  AC_DEFINE(CONFIG_SND_WAVEFRONT_FIRMWARE_IN_KERNEL)
fi
if test -n "$CONFIG_SND_PC98_CS4232"; then
  AC_DEFINE(CONFIG_SND_PC98_CS4232_MODULE)
fi
if test -n "$CONFIG_SND_MSND_PINNACLE"; then
  AC_DEFINE(CONFIG_SND_MSND_PINNACLE_MODULE)
fi
if test -n "$CONFIG_SND_AD1889"; then
  AC_DEFINE(CONFIG_SND_AD1889_MODULE)
fi
if test -n "$CONFIG_SND_ALS300"; then
  AC_DEFINE(CONFIG_SND_ALS300_MODULE)
fi
if test -n "$CONFIG_SND_ALS4000"; then
  AC_DEFINE(CONFIG_SND_ALS4000_MODULE)
fi
if test -n "$CONFIG_SND_ALI5451"; then
  AC_DEFINE(CONFIG_SND_ALI5451_MODULE)
fi
if test -n "$CONFIG_SND_ATIIXP"; then
  AC_DEFINE(CONFIG_SND_ATIIXP_MODULE)
fi
if test -n "$CONFIG_SND_ATIIXP_MODEM"; then
  AC_DEFINE(CONFIG_SND_ATIIXP_MODEM_MODULE)
fi
if test -n "$CONFIG_SND_AU8810"; then
  AC_DEFINE(CONFIG_SND_AU8810_MODULE)
fi
if test -n "$CONFIG_SND_AU8820"; then
  AC_DEFINE(CONFIG_SND_AU8820_MODULE)
fi
if test -n "$CONFIG_SND_AU8830"; then
  AC_DEFINE(CONFIG_SND_AU8830_MODULE)
fi
if test -n "$CONFIG_SND_AZT3328"; then
  AC_DEFINE(CONFIG_SND_AZT3328_MODULE)
fi
if test -n "$CONFIG_SND_BT87X"; then
  AC_DEFINE(CONFIG_SND_BT87X_MODULE)
fi
if test -n "$CONFIG_SND_BT87X_OVERCLOCK"; then
  AC_DEFINE(CONFIG_SND_BT87X_OVERCLOCK)
fi
if test -n "$CONFIG_SND_CA0106"; then
  AC_DEFINE(CONFIG_SND_CA0106_MODULE)
fi
if test -n "$CONFIG_SND_CMIPCI"; then
  AC_DEFINE(CONFIG_SND_CMIPCI_MODULE)
fi
if test -n "$CONFIG_SND_OXYGEN_LIB"; then
  AC_DEFINE(CONFIG_SND_OXYGEN_LIB_MODULE)
fi
if test -n "$CONFIG_SND_OXYGEN"; then
  AC_DEFINE(CONFIG_SND_OXYGEN_MODULE)
fi
if test -n "$CONFIG_SND_CS4281"; then
  AC_DEFINE(CONFIG_SND_CS4281_MODULE)
fi
if test -n "$CONFIG_SND_CS46XX"; then
  AC_DEFINE(CONFIG_SND_CS46XX_MODULE)
fi
if test -n "$CONFIG_SND_CS46XX_NEW_DSP"; then
  AC_DEFINE(CONFIG_SND_CS46XX_NEW_DSP)
fi
if test -n "$CONFIG_SND_CS5530"; then
  AC_DEFINE(CONFIG_SND_CS5530_MODULE)
fi
if test -n "$CONFIG_SND_CS5535AUDIO"; then
  AC_DEFINE(CONFIG_SND_CS5535AUDIO_MODULE)
fi
if test -n "$CONFIG_SND_DARLA20"; then
  AC_DEFINE(CONFIG_SND_DARLA20_MODULE)
fi
if test -n "$CONFIG_SND_GINA20"; then
  AC_DEFINE(CONFIG_SND_GINA20_MODULE)
fi
if test -n "$CONFIG_SND_LAYLA20"; then
  AC_DEFINE(CONFIG_SND_LAYLA20_MODULE)
fi
if test -n "$CONFIG_SND_DARLA24"; then
  AC_DEFINE(CONFIG_SND_DARLA24_MODULE)
fi
if test -n "$CONFIG_SND_GINA24"; then
  AC_DEFINE(CONFIG_SND_GINA24_MODULE)
fi
if test -n "$CONFIG_SND_LAYLA24"; then
  AC_DEFINE(CONFIG_SND_LAYLA24_MODULE)
fi
if test -n "$CONFIG_SND_MONA"; then
  AC_DEFINE(CONFIG_SND_MONA_MODULE)
fi
if test -n "$CONFIG_SND_MIA"; then
  AC_DEFINE(CONFIG_SND_MIA_MODULE)
fi
if test -n "$CONFIG_SND_ECHO3G"; then
  AC_DEFINE(CONFIG_SND_ECHO3G_MODULE)
fi
if test -n "$CONFIG_SND_INDIGO"; then
  AC_DEFINE(CONFIG_SND_INDIGO_MODULE)
fi
if test -n "$CONFIG_SND_INDIGOIO"; then
  AC_DEFINE(CONFIG_SND_INDIGOIO_MODULE)
fi
if test -n "$CONFIG_SND_INDIGODJ"; then
  AC_DEFINE(CONFIG_SND_INDIGODJ_MODULE)
fi
if test -n "$CONFIG_SND_EMU10K1"; then
  AC_DEFINE(CONFIG_SND_EMU10K1_MODULE)
fi
if test -n "$CONFIG_SND_EMU10K1X"; then
  AC_DEFINE(CONFIG_SND_EMU10K1X_MODULE)
fi
if test -n "$CONFIG_SND_ENS1370"; then
  AC_DEFINE(CONFIG_SND_ENS1370_MODULE)
fi
if test -n "$CONFIG_SND_ENS1371"; then
  AC_DEFINE(CONFIG_SND_ENS1371_MODULE)
fi
if test -n "$CONFIG_SND_ES1938"; then
  AC_DEFINE(CONFIG_SND_ES1938_MODULE)
fi
if test -n "$CONFIG_SND_ES1968"; then
  AC_DEFINE(CONFIG_SND_ES1968_MODULE)
fi
if test -n "$CONFIG_SND_FM801"; then
  AC_DEFINE(CONFIG_SND_FM801_MODULE)
fi
if test -n "$CONFIG_SND_FM801_TEA575X_BOOL"; then
  AC_DEFINE(CONFIG_SND_FM801_TEA575X_BOOL)
fi
if test -n "$CONFIG_SND_FM801_TEA575X"; then
  AC_DEFINE(CONFIG_SND_FM801_TEA575X_MODULE)
fi
if test -n "$CONFIG_SND_HDA_INTEL"; then
  AC_DEFINE(CONFIG_SND_HDA_INTEL_MODULE)
fi
if test -n "$CONFIG_SND_HDA_HWDEP"; then
  AC_DEFINE(CONFIG_SND_HDA_HWDEP)
fi
if test -n "$CONFIG_SND_HDA_CODEC_REALTEK"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_REALTEK)
fi
if test -n "$CONFIG_SND_HDA_CODEC_ANALOG"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_ANALOG)
fi
if test -n "$CONFIG_SND_HDA_CODEC_SIGMATEL"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_SIGMATEL)
fi
if test -n "$CONFIG_SND_HDA_CODEC_VIA"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_VIA)
fi
if test -n "$CONFIG_SND_HDA_CODEC_ATIHDMI"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_ATIHDMI)
fi
if test -n "$CONFIG_SND_HDA_CODEC_CONEXANT"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_CONEXANT)
fi
if test -n "$CONFIG_SND_HDA_CODEC_CMEDIA"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_CMEDIA)
fi
if test -n "$CONFIG_SND_HDA_CODEC_SI3054"; then
  AC_DEFINE(CONFIG_SND_HDA_CODEC_SI3054)
fi
if test -n "$CONFIG_SND_HDA_GENERIC"; then
  AC_DEFINE(CONFIG_SND_HDA_GENERIC)
fi
if test -n "$CONFIG_SND_HDA_POWER_SAVE"; then
  AC_DEFINE(CONFIG_SND_HDA_POWER_SAVE)
fi
if test -n "$CONFIG_SND_HDA_POWER_SAVE_DEFAULT"; then
  AC_DEFINE_UNQUOTED([CONFIG_SND_HDA_POWER_SAVE_DEFAULT], [0])
fi
if test -n "$CONFIG_SND_HDSP"; then
  AC_DEFINE(CONFIG_SND_HDSP_MODULE)
fi
if test -n "$CONFIG_SND_HDSPM"; then
  AC_DEFINE(CONFIG_SND_HDSPM_MODULE)
fi
if test -n "$CONFIG_SND_HIFIER"; then
  AC_DEFINE(CONFIG_SND_HIFIER_MODULE)
fi
if test -n "$CONFIG_SND_ICE1712"; then
  AC_DEFINE(CONFIG_SND_ICE1712_MODULE)
fi
if test -n "$CONFIG_SND_ICE1724"; then
  AC_DEFINE(CONFIG_SND_ICE1724_MODULE)
fi
if test -n "$CONFIG_SND_INTEL8X0"; then
  AC_DEFINE(CONFIG_SND_INTEL8X0_MODULE)
fi
if test -n "$CONFIG_SND_INTEL8X0M"; then
  AC_DEFINE(CONFIG_SND_INTEL8X0M_MODULE)
fi
if test -n "$CONFIG_SND_KORG1212"; then
  AC_DEFINE(CONFIG_SND_KORG1212_MODULE)
fi
if test -n "$CONFIG_SND_KORG1212_FIRMWARE_IN_KERNEL"; then
  AC_DEFINE(CONFIG_SND_KORG1212_FIRMWARE_IN_KERNEL)
fi
if test -n "$CONFIG_SND_MAESTRO3"; then
  AC_DEFINE(CONFIG_SND_MAESTRO3_MODULE)
fi
if test -n "$CONFIG_SND_MAESTRO3_FIRMWARE_IN_KERNEL"; then
  AC_DEFINE(CONFIG_SND_MAESTRO3_FIRMWARE_IN_KERNEL)
fi
if test -n "$CONFIG_SND_MIXART"; then
  AC_DEFINE(CONFIG_SND_MIXART_MODULE)
fi
if test -n "$CONFIG_SND_NM256"; then
  AC_DEFINE(CONFIG_SND_NM256_MODULE)
fi
if test -n "$CONFIG_SND_PCXHR"; then
  AC_DEFINE(CONFIG_SND_PCXHR_MODULE)
fi
if test -n "$CONFIG_SND_RIPTIDE"; then
  AC_DEFINE(CONFIG_SND_RIPTIDE_MODULE)
fi
if test -n "$CONFIG_SND_RME32"; then
  AC_DEFINE(CONFIG_SND_RME32_MODULE)
fi
if test -n "$CONFIG_SND_RME96"; then
  AC_DEFINE(CONFIG_SND_RME96_MODULE)
fi
if test -n "$CONFIG_SND_RME9652"; then
  AC_DEFINE(CONFIG_SND_RME9652_MODULE)
fi
if test -n "$CONFIG_SND_SIS7019"; then
  AC_DEFINE(CONFIG_SND_SIS7019_MODULE)
fi
if test -n "$CONFIG_SND_SONICVIBES"; then
  AC_DEFINE(CONFIG_SND_SONICVIBES_MODULE)
fi
if test -n "$CONFIG_SND_TRIDENT"; then
  AC_DEFINE(CONFIG_SND_TRIDENT_MODULE)
fi
if test -n "$CONFIG_SND_VIA82XX"; then
  AC_DEFINE(CONFIG_SND_VIA82XX_MODULE)
fi
if test -n "$CONFIG_SND_VIA82XX_MODEM"; then
  AC_DEFINE(CONFIG_SND_VIA82XX_MODEM_MODULE)
fi
if test -n "$CONFIG_SND_VIRTUOSO"; then
  AC_DEFINE(CONFIG_SND_VIRTUOSO_MODULE)
fi
if test -n "$CONFIG_SND_VX222"; then
  AC_DEFINE(CONFIG_SND_VX222_MODULE)
fi
if test -n "$CONFIG_SND_YMFPCI"; then
  AC_DEFINE(CONFIG_SND_YMFPCI_MODULE)
fi
if test -n "$CONFIG_SND_YMFPCI_FIRMWARE_IN_KERNEL"; then
  AC_DEFINE(CONFIG_SND_YMFPCI_FIRMWARE_IN_KERNEL)
fi
if test -n "$CONFIG_SND_AC97_POWER_SAVE"; then
  AC_DEFINE(CONFIG_SND_AC97_POWER_SAVE)
fi
if test -n "$CONFIG_SND_AC97_POWER_SAVE_DEFAULT"; then
  AC_DEFINE_UNQUOTED([CONFIG_SND_AC97_POWER_SAVE_DEFAULT], [0])
fi
if test -n "$CONFIG_SND_PDPLUS"; then
  AC_DEFINE(CONFIG_SND_PDPLUS_MODULE)
fi
if test -n "$CONFIG_SND_ASIHPI"; then
  AC_DEFINE(CONFIG_SND_ASIHPI_MODULE)
fi
if test -n "$CONFIG_SND_POWERMAC"; then
  AC_DEFINE(CONFIG_SND_POWERMAC_MODULE)
fi
if test -n "$CONFIG_SND_POWERMAC_AUTO_DRC"; then
  AC_DEFINE(CONFIG_SND_POWERMAC_AUTO_DRC)
fi
if test -n "$CONFIG_PPC32"; then
  AC_DEFINE(CONFIG_PPC32_MODULE)
fi
if test -n "$CONFIG_SND_PS3"; then
  AC_DEFINE(CONFIG_SND_PS3_MODULE)
fi
if test -n "$CONFIG_PS3_PS3AV"; then
  AC_DEFINE(CONFIG_PS3_PS3AV_MODULE)
fi
if test -n "$CONFIG_SND_PS3_DEFAULT_START_DELAY"; then
  AC_DEFINE_UNQUOTED([CONFIG_SND_PS3_DEFAULT_START_DELAY], [2000])
fi
if test -n "$CONFIG_SND_AOA"; then
  AC_DEFINE(CONFIG_SND_AOA_MODULE)
fi
if test -n "$CONFIG_SND_AOA_FABRIC_LAYOUT"; then
  AC_DEFINE(CONFIG_SND_AOA_FABRIC_LAYOUT_MODULE)
fi
if test -n "$CONFIG_SND_AOA_ONYX"; then
  AC_DEFINE(CONFIG_SND_AOA_ONYX_MODULE)
fi
if test -n "$CONFIG_I2C_POWERMAC"; then
  AC_DEFINE(CONFIG_I2C_POWERMAC_MODULE)
fi
if test -n "$CONFIG_SND_AOA_TAS"; then
  AC_DEFINE(CONFIG_SND_AOA_TAS_MODULE)
fi
if test -n "$CONFIG_SND_AOA_TOONIE"; then
  AC_DEFINE(CONFIG_SND_AOA_TOONIE_MODULE)
fi
if test -n "$CONFIG_SND_AOA_SOUNDBUS"; then
  AC_DEFINE(CONFIG_SND_AOA_SOUNDBUS_MODULE)
fi
if test -n "$CONFIG_SND_AOA_SOUNDBUS_I2S"; then
  AC_DEFINE(CONFIG_SND_AOA_SOUNDBUS_I2S_MODULE)
fi
if test -n "$CONFIG_SND_SA11XX_UDA1341"; then
  AC_DEFINE(CONFIG_SND_SA11XX_UDA1341_MODULE)
fi
if test -n "$CONFIG_SND_ARMAACI"; then
  AC_DEFINE(CONFIG_SND_ARMAACI_MODULE)
fi
if test -n "$CONFIG_ARM_AMBA"; then
  AC_DEFINE(CONFIG_ARM_AMBA_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_PCM"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_PCM_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_AC97"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_AC97_MODULE)
fi
if test -n "$CONFIG_SND_S3C2410"; then
  AC_DEFINE(CONFIG_SND_S3C2410_MODULE)
fi
if test -n "$CONFIG_ARCH_S3C2410"; then
  AC_DEFINE(CONFIG_ARCH_S3C2410_MODULE)
fi
if test -n "$CONFIG_I2C_SENSOR"; then
  AC_DEFINE(CONFIG_I2C_SENSOR_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_I2SOUND"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_I2SOUND_MODULE)
fi
if test -n "$CONFIG_SND_AT73C213"; then
  AC_DEFINE(CONFIG_SND_AT73C213_MODULE)
fi
if test -n "$CONFIG_ATMEL_SSC"; then
  AC_DEFINE(CONFIG_ATMEL_SSC_MODULE)
fi
if test -n "$CONFIG_SND_AT73C213_TARGET_BITRATE"; then
  AC_DEFINE_UNQUOTED([CONFIG_SND_AT73C213_TARGET_BITRATE], [48000])
fi
if test -n "$CONFIG_SND_AU1X00"; then
  AC_DEFINE(CONFIG_SND_AU1X00_MODULE)
fi
if test -n "$CONFIG_SOC_AU1000"; then
  AC_DEFINE(CONFIG_SOC_AU1000_MODULE)
fi
if test -n "$CONFIG_SOC_AU1100"; then
  AC_DEFINE(CONFIG_SOC_AU1100_MODULE)
fi
if test -n "$CONFIG_SOC_AU1500"; then
  AC_DEFINE(CONFIG_SOC_AU1500_MODULE)
fi
if test -n "$CONFIG_SND_AICA"; then
  AC_DEFINE(CONFIG_SND_AICA_MODULE)
fi
if test -n "$CONFIG_SND_USB_AUDIO"; then
  AC_DEFINE(CONFIG_SND_USB_AUDIO_MODULE)
fi
if test -n "$CONFIG_SND_USB_USX2Y"; then
  AC_DEFINE(CONFIG_SND_USB_USX2Y_MODULE)
fi
if test -n "$CONFIG_ALPHA"; then
  AC_DEFINE(CONFIG_ALPHA_MODULE)
fi
if test -n "$CONFIG_SND_USB_CAIAQ"; then
  AC_DEFINE(CONFIG_SND_USB_CAIAQ_MODULE)
fi
if test -n "$CONFIG_SND_USB_CAIAQ_INPUT"; then
  AC_DEFINE(CONFIG_SND_USB_CAIAQ_INPUT)
fi
if test -n "$CONFIG_SND_VXPOCKET"; then
  AC_DEFINE(CONFIG_SND_VXPOCKET_MODULE)
fi
if test -n "$CONFIG_SND_PDAUDIOCF"; then
  AC_DEFINE(CONFIG_SND_PDAUDIOCF_MODULE)
fi
if test -n "$CONFIG_SPARC"; then
  AC_DEFINE(CONFIG_SPARC_MODULE)
fi
if test -n "$CONFIG_SND_SUN_AMD7930"; then
  AC_DEFINE(CONFIG_SND_SUN_AMD7930_MODULE)
fi
if test -n "$CONFIG_SND_SUN_CS4231"; then
  AC_DEFINE(CONFIG_SND_SUN_CS4231_MODULE)
fi
if test -n "$CONFIG_SND_SUN_DBRI"; then
  AC_DEFINE(CONFIG_SND_SUN_DBRI_MODULE)
fi
if test -n "$CONFIG_GSC"; then
  AC_DEFINE(CONFIG_GSC_MODULE)
fi
if test -n "$CONFIG_SND_HARMONY"; then
  AC_DEFINE(CONFIG_SND_HARMONY_MODULE)
fi
if test -n "$CONFIG_SND_SOC_AC97_BUS"; then
  AC_DEFINE(CONFIG_SND_SOC_AC97_BUS)
fi
if test -n "$CONFIG_SND_SOC"; then
  AC_DEFINE(CONFIG_SND_SOC_MODULE)
fi
if test -n "$CONFIG_SND_AT91_SOC"; then
  AC_DEFINE(CONFIG_SND_AT91_SOC_MODULE)
fi
if test -n "$CONFIG_ARCH_AT91"; then
  AC_DEFINE(CONFIG_ARCH_AT91_MODULE)
fi
if test -n "$CONFIG_SND_AT91_SOC_SSC"; then
  AC_DEFINE(CONFIG_SND_AT91_SOC_SSC_MODULE)
fi
if test -n "$CONFIG_SND_AT91_SOC_ETI_B1_WM8731"; then
  AC_DEFINE(CONFIG_SND_AT91_SOC_ETI_B1_WM8731_MODULE)
fi
if test -n "$CONFIG_MACH_ETI_B1"; then
  AC_DEFINE(CONFIG_MACH_ETI_B1_MODULE)
fi
if test -n "$CONFIG_MACH_ETI_C1"; then
  AC_DEFINE(CONFIG_MACH_ETI_C1_MODULE)
fi
if test -n "$CONFIG_SND_AT91_SOC_ETI_SLAVE"; then
  AC_DEFINE(CONFIG_SND_AT91_SOC_ETI_SLAVE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_AC97"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_AC97_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_I2S"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_I2S_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_CORGI"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_CORGI_MODULE)
fi
if test -n "$CONFIG_PXA_SHARP_C7XX"; then
  AC_DEFINE(CONFIG_PXA_SHARP_C7XX_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_SPITZ"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_SPITZ_MODULE)
fi
if test -n "$CONFIG_PXA_SHARP_CXX00"; then
  AC_DEFINE(CONFIG_PXA_SHARP_CXX00_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_POODLE"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_POODLE_MODULE)
fi
if test -n "$CONFIG_MACH_POODLE"; then
  AC_DEFINE(CONFIG_MACH_POODLE_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_TOSA"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_TOSA_MODULE)
fi
if test -n "$CONFIG_MACH_TOSA"; then
  AC_DEFINE(CONFIG_MACH_TOSA_MODULE)
fi
if test -n "$CONFIG_SND_PXA2XX_SOC_E800"; then
  AC_DEFINE(CONFIG_SND_PXA2XX_SOC_E800_MODULE)
fi
if test -n "$CONFIG_MACH_E800"; then
  AC_DEFINE(CONFIG_MACH_E800_MODULE)
fi
if test -n "$CONFIG_SND_S3C24XX_SOC"; then
  AC_DEFINE(CONFIG_SND_S3C24XX_SOC_MODULE)
fi
if test -n "$CONFIG_SND_S3C24XX_SOC_I2S"; then
  AC_DEFINE(CONFIG_SND_S3C24XX_SOC_I2S_MODULE)
fi
if test -n "$CONFIG_SND_S3C2412_SOC_I2S"; then
  AC_DEFINE(CONFIG_SND_S3C2412_SOC_I2S_MODULE)
fi
if test -n "$CONFIG_SND_S3C2443_SOC_AC97"; then
  AC_DEFINE(CONFIG_SND_S3C2443_SOC_AC97_MODULE)
fi
if test -n "$CONFIG_SND_S3C24XX_SOC_NEO1973_WM8753"; then
  AC_DEFINE(CONFIG_SND_S3C24XX_SOC_NEO1973_WM8753_MODULE)
fi
if test -n "$CONFIG_MACH_NEO1973_GTA01"; then
  AC_DEFINE(CONFIG_MACH_NEO1973_GTA01_MODULE)
fi
if test -n "$CONFIG_SND_S3C24XX_SOC_SMDK2443_WM9710"; then
  AC_DEFINE(CONFIG_SND_S3C24XX_SOC_SMDK2443_WM9710_MODULE)
fi
if test -n "$CONFIG_MACH_SMDK2443"; then
  AC_DEFINE(CONFIG_MACH_SMDK2443_MODULE)
fi
if test -n "$CONFIG_SND_S3C24XX_SOC_LN2440SBC_ALC650"; then
  AC_DEFINE(CONFIG_SND_S3C24XX_SOC_LN2440SBC_ALC650_MODULE)
fi
if test -n "$CONFIG_SND_SOC_PCM_SH7760"; then
  AC_DEFINE(CONFIG_SND_SOC_PCM_SH7760_MODULE)
fi
if test -n "$CONFIG_CPU_SUBTYPE_SH7760"; then
  AC_DEFINE(CONFIG_CPU_SUBTYPE_SH7760_MODULE)
fi
if test -n "$CONFIG_SH_DMABRG"; then
  AC_DEFINE(CONFIG_SH_DMABRG_MODULE)
fi
if test -n "$CONFIG_SND_SOC_SH4_HAC"; then
  AC_DEFINE(CONFIG_SND_SOC_SH4_HAC_MODULE)
fi
if test -n "$CONFIG_SND_SOC_SH4_SSI"; then
  AC_DEFINE(CONFIG_SND_SOC_SH4_SSI_MODULE)
fi
if test -n "$CONFIG_SND_SH7760_AC97"; then
  AC_DEFINE(CONFIG_SND_SH7760_AC97_MODULE)
fi
if test -n "$CONFIG_SND_SOC_MPC8610"; then
  AC_DEFINE(CONFIG_SND_SOC_MPC8610)
fi
if test -n "$CONFIG_MPC8610_HPCD"; then
  AC_DEFINE(CONFIG_MPC8610_HPCD_MODULE)
fi
if test -n "$CONFIG_SND_SOC_MPC8610_HPCD"; then
  AC_DEFINE(CONFIG_SND_SOC_MPC8610_HPCD)
fi
if test -n "$CONFIG_SND_SOC_AC97_CODEC"; then
  AC_DEFINE(CONFIG_SND_SOC_AC97_CODEC_MODULE)
fi
if test -n "$CONFIG_SND_SOC_WM8731"; then
  AC_DEFINE(CONFIG_SND_SOC_WM8731_MODULE)
fi
if test -n "$CONFIG_SND_SOC_WM8750"; then
  AC_DEFINE(CONFIG_SND_SOC_WM8750_MODULE)
fi
if test -n "$CONFIG_SND_SOC_WM8753"; then
  AC_DEFINE(CONFIG_SND_SOC_WM8753_MODULE)
fi
if test -n "$CONFIG_SND_SOC_WM9712"; then
  AC_DEFINE(CONFIG_SND_SOC_WM9712_MODULE)
fi
if test -n "$CONFIG_SND_SOC_CS4270"; then
  AC_DEFINE(CONFIG_SND_SOC_CS4270_MODULE)
fi
if test -n "$CONFIG_SND_SOC_CS4270_HWMUTE"; then
  AC_DEFINE(CONFIG_SND_SOC_CS4270_HWMUTE)
fi
if test -n "$CONFIG_SND_SOC_CS4270_VD33_ERRATA"; then
  AC_DEFINE(CONFIG_SND_SOC_CS4270_VD33_ERRATA)
fi
if test -n "$CONFIG_SND_SOC_TLV320AIC3X"; then
  AC_DEFINE(CONFIG_SND_SOC_TLV320AIC3X_MODULE)
fi
])

AC_DEFUN([ALSA_TOPLEVEL_OUTPUT], [
dnl output all subst
AC_SUBST(CONFIG_HAS_IOMEM)
AC_SUBST(CONFIG_SOUND)
AC_SUBST(CONFIG_SND)
AC_SUBST(CONFIG_SND_TIMER)
AC_SUBST(CONFIG_SND_PCM)
AC_SUBST(CONFIG_SND_HWDEP)
AC_SUBST(CONFIG_SND_RAWMIDI)
AC_SUBST(CONFIG_SND_SEQUENCER)
AC_SUBST(CONFIG_SND_SEQ_DUMMY)
AC_SUBST(CONFIG_SND_OSSEMUL)
AC_SUBST(CONFIG_SND_MIXER_OSS)
AC_SUBST(CONFIG_SND_PCM_OSS)
AC_SUBST(CONFIG_SND_PCM_OSS_PLUGINS)
AC_SUBST(CONFIG_SND_SEQUENCER_OSS)
AC_SUBST(CONFIG_SND_RTCTIMER)
AC_SUBST(CONFIG_RTC)
AC_SUBST(CONFIG_SND_SEQ_RTCTIMER_DEFAULT)
AC_SUBST(CONFIG_SND_DYNAMIC_MINORS)
AC_SUBST(CONFIG_SND_SUPPORT_OLD_API)
AC_SUBST(CONFIG_SND_VERBOSE_PROCFS)
AC_SUBST(CONFIG_PROC_FS)
AC_SUBST(CONFIG_SND_VERBOSE_PRINTK)
AC_SUBST(CONFIG_SND_DEBUG)
AC_SUBST(CONFIG_SND_DEBUG_DETECT)
AC_SUBST(CONFIG_SND_PCM_XRUN_DEBUG)
AC_SUBST(CONFIG_SND_BIT32_EMUL)
AC_SUBST(CONFIG_SND_DEBUG_MEMORY)
AC_SUBST(CONFIG_SND_HPET)
AC_SUBST(CONFIG_HPET)
AC_SUBST(CONFIG_SND_MPU401_UART)
AC_SUBST(CONFIG_SND_OPL3_LIB)
AC_SUBST(CONFIG_SND_OPL4_LIB)
AC_SUBST(CONFIG_SND_VX_LIB)
AC_SUBST(CONFIG_SND_AC97_CODEC)
AC_SUBST(CONFIG_SND_DUMMY)
AC_SUBST(CONFIG_SND_VIRMIDI)
AC_SUBST(CONFIG_SND_MTPAV)
AC_SUBST(CONFIG_SND_MTS64)
AC_SUBST(CONFIG_PARPORT)
AC_SUBST(CONFIG_SND_SERIAL_U16550)
AC_SUBST(CONFIG_SND_MPU401)
AC_SUBST(CONFIG_SND_PORTMAN2X4)
AC_SUBST(CONFIG_SND_ML403_AC97CR)
AC_SUBST(CONFIG_XILINX_VIRTEX)
AC_SUBST(CONFIG_SND_SERIALMIDI)
AC_SUBST(CONFIG_BROKEN)
AC_SUBST(CONFIG_SND_LOOPBACK)
AC_SUBST(CONFIG_SND_PCSP)
AC_SUBST(CONFIG_EXPERIMENTAL)
AC_SUBST(CONFIG_X86_PC)
AC_SUBST(CONFIG_HIGH_RES_TIMERS)
AC_SUBST(CONFIG_SND_AD1848_LIB)
AC_SUBST(CONFIG_SND_CS4231_LIB)
AC_SUBST(CONFIG_SND_SB_COMMON)
AC_SUBST(CONFIG_SND_SB8_DSP)
AC_SUBST(CONFIG_SND_SB16_DSP)
AC_SUBST(CONFIG_ISA)
AC_SUBST(CONFIG_ISA_DMA_API)
AC_SUBST(CONFIG_SND_ADLIB)
AC_SUBST(CONFIG_SND_AD1816A)
AC_SUBST(CONFIG_PNP)
AC_SUBST(CONFIG_ISAPNP)
AC_SUBST(CONFIG_SND_AD1848)
AC_SUBST(CONFIG_SND_ALS100)
AC_SUBST(CONFIG_SND_AZT2320)
AC_SUBST(CONFIG_SND_CMI8330)
AC_SUBST(CONFIG_SND_CS4231)
AC_SUBST(CONFIG_SND_CS4232)
AC_SUBST(CONFIG_SND_CS4236)
AC_SUBST(CONFIG_SND_DT019X)
AC_SUBST(CONFIG_SND_ES968)
AC_SUBST(CONFIG_SND_ES1688)
AC_SUBST(CONFIG_SND_ES18XX)
AC_SUBST(CONFIG_SND_SC6000)
AC_SUBST(CONFIG_HAS_IOPORT)
AC_SUBST(CONFIG_SND_GUS_SYNTH)
AC_SUBST(CONFIG_SND_GUSCLASSIC)
AC_SUBST(CONFIG_SND_GUSEXTREME)
AC_SUBST(CONFIG_SND_GUSMAX)
AC_SUBST(CONFIG_SND_INTERWAVE)
AC_SUBST(CONFIG_SND_INTERWAVE_STB)
AC_SUBST(CONFIG_SND_OPL3SA2)
AC_SUBST(CONFIG_SND_OPTI92X_AD1848)
AC_SUBST(CONFIG_SND_OPTI92X_CS4231)
AC_SUBST(CONFIG_SND_OPTI93X)
AC_SUBST(CONFIG_SND_MIRO)
AC_SUBST(CONFIG_SND_SB8)
AC_SUBST(CONFIG_SND_SB16)
AC_SUBST(CONFIG_SND_SBAWE)
AC_SUBST(CONFIG_SND_SB16_CSP)
AC_SUBST(CONFIG_PPC)
AC_SUBST(CONFIG_FW_LOADER)
AC_SUBST(CONFIG_SND_SB16_CSP_FIRMWARE_IN_KERNEL)
AC_SUBST(CONFIG_SND_SGALAXY)
AC_SUBST(CONFIG_SND_SSCAPE)
AC_SUBST(CONFIG_SND_WAVEFRONT)
AC_SUBST(CONFIG_SND_WAVEFRONT_FIRMWARE_IN_KERNEL)
AC_SUBST(CONFIG_SND_PC98_CS4232)
AC_SUBST(CONFIG_X86_PC9800)
AC_SUBST(CONFIG_SND_MSND_PINNACLE)
AC_SUBST(CONFIG_X86)
AC_SUBST(CONFIG_PCI)
AC_SUBST(CONFIG_SND_AD1889)
AC_SUBST(CONFIG_SND_ALS300)
AC_SUBST(CONFIG_SND_ALS4000)
AC_SUBST(CONFIG_SND_ALI5451)
AC_SUBST(CONFIG_SND_ATIIXP)
AC_SUBST(CONFIG_SND_ATIIXP_MODEM)
AC_SUBST(CONFIG_SND_AU8810)
AC_SUBST(CONFIG_SND_AU8820)
AC_SUBST(CONFIG_SND_AU8830)
AC_SUBST(CONFIG_SND_AZT3328)
AC_SUBST(CONFIG_SND_BT87X)
AC_SUBST(CONFIG_SND_BT87X_OVERCLOCK)
AC_SUBST(CONFIG_SND_CA0106)
AC_SUBST(CONFIG_SND_CMIPCI)
AC_SUBST(CONFIG_SND_OXYGEN_LIB)
AC_SUBST(CONFIG_SND_OXYGEN)
AC_SUBST(CONFIG_SND_CS4281)
AC_SUBST(CONFIG_SND_CS46XX)
AC_SUBST(CONFIG_SND_CS46XX_NEW_DSP)
AC_SUBST(CONFIG_SND_CS5530)
AC_SUBST(CONFIG_SND_CS5535AUDIO)
AC_SUBST(CONFIG_X86_64)
AC_SUBST(CONFIG_SND_DARLA20)
AC_SUBST(CONFIG_SND_GINA20)
AC_SUBST(CONFIG_SND_LAYLA20)
AC_SUBST(CONFIG_SND_DARLA24)
AC_SUBST(CONFIG_SND_GINA24)
AC_SUBST(CONFIG_SND_LAYLA24)
AC_SUBST(CONFIG_SND_MONA)
AC_SUBST(CONFIG_SND_MIA)
AC_SUBST(CONFIG_SND_ECHO3G)
AC_SUBST(CONFIG_SND_INDIGO)
AC_SUBST(CONFIG_SND_INDIGOIO)
AC_SUBST(CONFIG_SND_INDIGODJ)
AC_SUBST(CONFIG_SND_EMU10K1)
AC_SUBST(CONFIG_SND_EMU10K1X)
AC_SUBST(CONFIG_SND_ENS1370)
AC_SUBST(CONFIG_SND_ENS1371)
AC_SUBST(CONFIG_SND_ES1938)
AC_SUBST(CONFIG_SND_ES1968)
AC_SUBST(CONFIG_SND_FM801)
AC_SUBST(CONFIG_SND_FM801_TEA575X_BOOL)
AC_SUBST(CONFIG_SND_FM801_TEA575X)
AC_SUBST(CONFIG_VIDEO_V4L1)
AC_SUBST(CONFIG_VIDEO_DEV)
AC_SUBST(CONFIG_SND_HDA_INTEL)
AC_SUBST(CONFIG_SND_HDA_HWDEP)
AC_SUBST(CONFIG_SND_HDA_CODEC_REALTEK)
AC_SUBST(CONFIG_SND_HDA_CODEC_ANALOG)
AC_SUBST(CONFIG_SND_HDA_CODEC_SIGMATEL)
AC_SUBST(CONFIG_SND_HDA_CODEC_VIA)
AC_SUBST(CONFIG_SND_HDA_CODEC_ATIHDMI)
AC_SUBST(CONFIG_SND_HDA_CODEC_CONEXANT)
AC_SUBST(CONFIG_SND_HDA_CODEC_CMEDIA)
AC_SUBST(CONFIG_SND_HDA_CODEC_SI3054)
AC_SUBST(CONFIG_SND_HDA_GENERIC)
AC_SUBST(CONFIG_SND_HDA_POWER_SAVE)
AC_SUBST(CONFIG_SND_HDA_POWER_SAVE_DEFAULT)
AC_SUBST(CONFIG_SND_HDSP)
AC_SUBST(CONFIG_SND_HDSPM)
AC_SUBST(CONFIG_SND_HIFIER)
AC_SUBST(CONFIG_SND_ICE1712)
AC_SUBST(CONFIG_SND_ICE1724)
AC_SUBST(CONFIG_SND_INTEL8X0)
AC_SUBST(CONFIG_SND_INTEL8X0M)
AC_SUBST(CONFIG_SND_KORG1212)
AC_SUBST(CONFIG_SND_KORG1212_FIRMWARE_IN_KERNEL)
AC_SUBST(CONFIG_SND_MAESTRO3)
AC_SUBST(CONFIG_SND_MAESTRO3_FIRMWARE_IN_KERNEL)
AC_SUBST(CONFIG_SND_MIXART)
AC_SUBST(CONFIG_SND_NM256)
AC_SUBST(CONFIG_SND_PCXHR)
AC_SUBST(CONFIG_SND_RIPTIDE)
AC_SUBST(CONFIG_SND_RME32)
AC_SUBST(CONFIG_SND_RME96)
AC_SUBST(CONFIG_SND_RME9652)
AC_SUBST(CONFIG_SND_SIS7019)
AC_SUBST(CONFIG_SND_SONICVIBES)
AC_SUBST(CONFIG_SND_TRIDENT)
AC_SUBST(CONFIG_SND_VIA82XX)
AC_SUBST(CONFIG_SND_VIA82XX_MODEM)
AC_SUBST(CONFIG_SND_VIRTUOSO)
AC_SUBST(CONFIG_SND_VX222)
AC_SUBST(CONFIG_SND_YMFPCI)
AC_SUBST(CONFIG_SND_YMFPCI_FIRMWARE_IN_KERNEL)
AC_SUBST(CONFIG_SND_AC97_POWER_SAVE)
AC_SUBST(CONFIG_SND_AC97_POWER_SAVE_DEFAULT)
AC_SUBST(CONFIG_SND_PDPLUS)
AC_SUBST(CONFIG_SND_ASIHPI)
AC_SUBST(CONFIG_SND_POWERMAC)
AC_SUBST(CONFIG_I2C)
AC_SUBST(CONFIG_INPUT)
AC_SUBST(CONFIG_PPC_PMAC)
AC_SUBST(CONFIG_SND_POWERMAC_AUTO_DRC)
AC_SUBST(CONFIG_PPC64)
AC_SUBST(CONFIG_PPC32)
AC_SUBST(CONFIG_SND_PS3)
AC_SUBST(CONFIG_PS3_PS3AV)
AC_SUBST(CONFIG_SND_PS3_DEFAULT_START_DELAY)
AC_SUBST(CONFIG_SND_AOA)
AC_SUBST(CONFIG_SND_AOA_FABRIC_LAYOUT)
AC_SUBST(CONFIG_SND_AOA_ONYX)
AC_SUBST(CONFIG_I2C_POWERMAC)
AC_SUBST(CONFIG_SND_AOA_TAS)
AC_SUBST(CONFIG_SND_AOA_TOONIE)
AC_SUBST(CONFIG_SND_AOA_SOUNDBUS)
AC_SUBST(CONFIG_SND_AOA_SOUNDBUS_I2S)
AC_SUBST(CONFIG_ARM)
AC_SUBST(CONFIG_SND_SA11XX_UDA1341)
AC_SUBST(CONFIG_ARCH_SA1100)
AC_SUBST(CONFIG_L3)
AC_SUBST(CONFIG_SND_ARMAACI)
AC_SUBST(CONFIG_ARM_AMBA)
AC_SUBST(CONFIG_SND_PXA2XX_PCM)
AC_SUBST(CONFIG_SND_PXA2XX_AC97)
AC_SUBST(CONFIG_ARCH_PXA)
AC_SUBST(CONFIG_SND_S3C2410)
AC_SUBST(CONFIG_ARCH_S3C2410)
AC_SUBST(CONFIG_I2C_SENSOR)
AC_SUBST(CONFIG_SND_PXA2XX_I2SOUND)
AC_SUBST(CONFIG_SND_AT73C213)
AC_SUBST(CONFIG_ATMEL_SSC)
AC_SUBST(CONFIG_SND_AT73C213_TARGET_BITRATE)
AC_SUBST(CONFIG_MIPS)
AC_SUBST(CONFIG_SND_AU1X00)
AC_SUBST(CONFIG_SOC_AU1000)
AC_SUBST(CONFIG_SOC_AU1100)
AC_SUBST(CONFIG_SOC_AU1500)
AC_SUBST(CONFIG_SUPERH)
AC_SUBST(CONFIG_SND_AICA)
AC_SUBST(CONFIG_SH_DREAMCAST)
AC_SUBST(CONFIG_USB)
AC_SUBST(CONFIG_SND_USB_AUDIO)
AC_SUBST(CONFIG_SND_USB_USX2Y)
AC_SUBST(CONFIG_ALPHA)
AC_SUBST(CONFIG_SND_USB_CAIAQ)
AC_SUBST(CONFIG_SND_USB_CAIAQ_INPUT)
AC_SUBST(CONFIG_PCMCIA)
AC_SUBST(CONFIG_SND_VXPOCKET)
AC_SUBST(CONFIG_SND_PDAUDIOCF)
AC_SUBST(CONFIG_SPARC)
AC_SUBST(CONFIG_SND_SUN_AMD7930)
AC_SUBST(CONFIG_SBUS)
AC_SUBST(CONFIG_SND_SUN_CS4231)
AC_SUBST(CONFIG_SND_SUN_DBRI)
AC_SUBST(CONFIG_GSC)
AC_SUBST(CONFIG_SND_HARMONY)
AC_SUBST(CONFIG_SND_SOC_AC97_BUS)
AC_SUBST(CONFIG_SND_SOC)
AC_SUBST(CONFIG_SND_AT91_SOC)
AC_SUBST(CONFIG_ARCH_AT91)
AC_SUBST(CONFIG_SND_AT91_SOC_SSC)
AC_SUBST(CONFIG_SND_AT91_SOC_ETI_B1_WM8731)
AC_SUBST(CONFIG_MACH_ETI_B1)
AC_SUBST(CONFIG_MACH_ETI_C1)
AC_SUBST(CONFIG_SND_AT91_SOC_ETI_SLAVE)
AC_SUBST(CONFIG_SND_PXA2XX_SOC)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_AC97)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_I2S)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_CORGI)
AC_SUBST(CONFIG_PXA_SHARP_C7XX)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_SPITZ)
AC_SUBST(CONFIG_PXA_SHARP_CXX00)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_POODLE)
AC_SUBST(CONFIG_MACH_POODLE)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_TOSA)
AC_SUBST(CONFIG_MACH_TOSA)
AC_SUBST(CONFIG_SND_PXA2XX_SOC_E800)
AC_SUBST(CONFIG_MACH_E800)
AC_SUBST(CONFIG_SND_S3C24XX_SOC)
AC_SUBST(CONFIG_SND_S3C24XX_SOC_I2S)
AC_SUBST(CONFIG_SND_S3C2412_SOC_I2S)
AC_SUBST(CONFIG_SND_S3C2443_SOC_AC97)
AC_SUBST(CONFIG_SND_S3C24XX_SOC_NEO1973_WM8753)
AC_SUBST(CONFIG_MACH_NEO1973_GTA01)
AC_SUBST(CONFIG_SND_S3C24XX_SOC_SMDK2443_WM9710)
AC_SUBST(CONFIG_MACH_SMDK2443)
AC_SUBST(CONFIG_SND_S3C24XX_SOC_LN2440SBC_ALC650)
AC_SUBST(CONFIG_SND_SOC_PCM_SH7760)
AC_SUBST(CONFIG_CPU_SUBTYPE_SH7760)
AC_SUBST(CONFIG_SH_DMABRG)
AC_SUBST(CONFIG_SND_SOC_SH4_HAC)
AC_SUBST(CONFIG_SND_SOC_SH4_SSI)
AC_SUBST(CONFIG_SND_SH7760_AC97)
AC_SUBST(CONFIG_SND_SOC_MPC8610)
AC_SUBST(CONFIG_MPC8610_HPCD)
AC_SUBST(CONFIG_SND_SOC_MPC8610_HPCD)
AC_SUBST(CONFIG_SND_SOC_AC97_CODEC)
AC_SUBST(CONFIG_SND_SOC_WM8731)
AC_SUBST(CONFIG_SND_SOC_WM8750)
AC_SUBST(CONFIG_SND_SOC_WM8753)
AC_SUBST(CONFIG_SND_SOC_WM9712)
AC_SUBST(CONFIG_SND_SOC_CS4270)
AC_SUBST(CONFIG_SND_SOC_CS4270_HWMUTE)
AC_SUBST(CONFIG_SND_SOC_CS4270_VD33_ERRATA)
AC_SUBST(CONFIG_SND_SOC_TLV320AIC3X)
AC_SUBST(CONFIG_SOUND_PRIME)
AC_SUBST(CONFIG_AC97_BUS)
])

