/***********************************************************************
 *
 * File: include/stmdisplayoutput.h
 * Copyright (c) 2000, 2004, 2005-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_OUTPUT_H
#define _STM_DISPLAY_OUTPUT_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum
{
  STVTG_TIMING_MODE_480I60000_13514,       /* NTSC I60 */
  STVTG_TIMING_MODE_480P60000_27027,       /* NTSC P60 */
  STVTG_TIMING_MODE_480I59940_13500,       /* NTSC */
  STVTG_TIMING_MODE_480P59940_27000,       /* NTSC P59.94 (525p ?) */
  STVTG_TIMING_MODE_480I59940_12273,       /* NTSC square, PAL M square */
  STVTG_TIMING_MODE_576I50000_13500,       /* PAL B,D,G,H,I,N, SECAM */
  STVTG_TIMING_MODE_576P50000_27000,       /* PAL P50 (625p ?)*/
  STVTG_TIMING_MODE_576I50000_14750,       /* PAL B,D,G,H,I,N, SECAM square */

  STVTG_TIMING_MODE_1080P60000_148500,     /* SMPTE 274M #1  P60 */
  STVTG_TIMING_MODE_1080P59940_148352,     /* SMPTE 274M #2  P60 /1.001 */
  STVTG_TIMING_MODE_1080P50000_148500,     /* SMPTE 274M #3  P50 */
  STVTG_TIMING_MODE_1080P30000_74250,      /* SMPTE 274M #7  P30 */
  STVTG_TIMING_MODE_1080P29970_74176,      /* SMPTE 274M #8  P30 /1.001 */
  STVTG_TIMING_MODE_1080P25000_74250,      /* SMPTE 274M #9  P25 */
  STVTG_TIMING_MODE_1080P24000_74250,      /* SMPTE 274M #10 P24 */
  STVTG_TIMING_MODE_1080P23976_74176,      /* SMPTE 274M #11 P24 /1.001 */

  STVTG_TIMING_MODE_1080I60000_74250,      /* EIA770.3 #3 I60 = SMPTE274M #4 I60 */
  STVTG_TIMING_MODE_1080I59940_74176,      /* EIA770.3 #4 I60 /1.001 = SMPTE274M #5 I60 /1.001 */
  STVTG_TIMING_MODE_1080I50000_74250_274M, /* SMPTE 274M Styled 1920x1080 I50 CEA/HDMI Code 20 */
  STVTG_TIMING_MODE_1080I50000_72000,      /* AS 4933.1-2005 1920x1080 I50 CEA/HDMI Code 39 */

  STVTG_TIMING_MODE_720P60000_74250,       /* EIA770.3 #1 P60 = SMPTE 296M #1 P60 */
  STVTG_TIMING_MODE_720P59940_74176,       /* EIA770.3 #2 P60 /1.001= SMPTE 296M #2 P60 /1.001 */
  STVTG_TIMING_MODE_720P50000_74250,       /* SMPTE 296M Styled 1280x720 50P CEA/HDMI Code 19 */

  STVTG_TIMING_MODE_1152I50000_48000,      /* AS 4933.1 1280x1152 I50 */

  STVTG_TIMING_MODE_480P59940_25180,       /* 640x480p @ 59.94Hz                    */
  STVTG_TIMING_MODE_480P60000_25200,       /* 640x480p @ 60Hz                       */

  STVTG_TIMING_MODE_480I59940_27000,       /* 480i @ 59.94Hz pixel doubled for HDMI */
  STVTG_TIMING_MODE_576I50000_27000,       /* 576i @ 50Hz pixel doubled for HDMI    */
  STVTG_TIMING_MODE_480I59940_54000,       /* 480i @ 59.94Hz pixel quaded for HDMI  */
  STVTG_TIMING_MODE_576I50000_54000,       /* 576i @ 50Hz pixel quaded for HDMI     */
  STVTG_TIMING_MODE_480P59940_54000,       /* 480p @ 59.94Hz pixel doubled for HDMI */
  STVTG_TIMING_MODE_480P60000_54054,       /* 480p @ 60Hz pixel doubled for HDMI    */
  STVTG_TIMING_MODE_576P50000_54000,       /* 576p @ 50Hz pixel doubled for HDMI    */
  STVTG_TIMING_MODE_480P59940_108000,      /* 480p @ 59.94Hz pixel quaded for HDMI  */
  STVTG_TIMING_MODE_480P60000_108108,      /* 480p @ 60Hz pixel doubled for HDMI    */
  STVTG_TIMING_MODE_576P50000_108000,      /* 576p @ 50Hz pixel quaded for HDMI     */

  STVTG_TIMING_MODE_COUNT                  /* must stay last */
} stm_display_mode_t;


typedef enum
{
  STVTG_SYNC_TIMING_MODE, /* Sync polarity follows the actual display mode        */
  STVTG_SYNC_POSITIVE,    /* Force H and V syncs positive                         */
  STVTG_SYNC_NEGATIVE,    /* Force H and V syncs negative                         */
  STVTG_SYNC_TOP_NOT_BOT, /* Positive H sync but V sync programmed as Top not Bot */
  STVTG_SYNC_BOT_NOT_TOP  /* Positive H sync but V sync programmed as Bot not Top */
} stm_vtg_sync_type_t;


typedef enum
{
  SCAN_P = 1,
  SCAN_I = 2
} stm_scan_type_t;


typedef struct
{
  ULONG          PixelsPerLine;
  ULONG          LinesByFrame;
  ULONG          ulPixelClock;
  BOOL           HSyncPolarity;
  ULONG          HSyncPulseWidth;
  BOOL           VSyncPolarity;
  ULONG          VSyncPulseWidth;
} stm_timing_params_t;


typedef enum
{
  AR_INDEX_4_3 = 0,
  AR_INDEX_16_9,
  AR_INDEX_COUNT
} stm_mode_aspect_ratio_index_t;


typedef struct
{
  ULONG           FrameRate;
  stm_scan_type_t ScanType;
  ULONG           ActiveAreaWidth;
  ULONG           ActiveAreaHeight;
  ULONG           ActiveAreaXStart;
  ULONG           FullVBIHeight;
  ULONG           OutputStandards;
  stm_rational_t  PixelAspectRatios[AR_INDEX_COUNT];
  ULONG           HDMIVideoCodes[AR_INDEX_COUNT];
} stm_mode_params_t;


typedef struct
{
  stm_display_mode_t  Mode;
  stm_mode_params_t   ModeParams;
  stm_timing_params_t TimingParams;
} stm_mode_line_t;


/*
 * Output capability flags.
 */
#define STM_OUTPUT_CAPS_SD_ANALOGUE      (1L<<0)
#define STM_OUTPUT_CAPS_ED_ANALOGUE      (1L<<1)
#define STM_OUTPUT_CAPS_HD_ANALOGUE      (1L<<2)

#define STM_OUTPUT_CAPS_DVO_656          (1L<<3)
#define STM_OUTPUT_CAPS_DVO_HD           (1L<<4)
#define STM_OUTPUT_CAPS_DVO_24BIT        (1L<<5)
#define STM_OUTPUT_CAPS_TMDS             (1L<<6)
#define STM_OUTPUT_CAPS_TMDS_DEEPCOLOUR  (1L<<7)

#define STM_OUTPUT_CAPS_MODE_CHANGE      (1L<<8)
#define STM_OUTPUT_CAPS_PLANE_MIXER      (1L<<9)
#define STM_OUTPUT_CAPS_BLIT_COMPOSITION (1L<<10)

#define STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE (1L<<11) /* Component DACs disabled                                            */
#define STM_OUTPUT_CAPS_YPrPb_EXCLUSIVE   (1L<<12) /* ED/HD component YPrPb, or SD component with CVBS&Y/C DACs disabled */
#define STM_OUTPUT_CAPS_RGB_EXCLUSIVE     (1L<<13) /* VGA/D-Sub, RGB on component output with no embedded syncs          */
#define STM_OUTPUT_CAPS_SD_RGB_CVBS_YC    (1L<<14) /* SCART, RGB+CVBS Y/C optionally disabled                            */
#define STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC  (1L<<15) /* Component YPrPb and composite outputs supported together           */

/*
 * Output standards
 */
#define STM_OUTPUT_STD_PAL_BDGHI     (1L<<0)
#define STM_OUTPUT_STD_PAL_M         (1L<<1)
#define STM_OUTPUT_STD_PAL_N         (1L<<2)
#define STM_OUTPUT_STD_PAL_Nc        (1L<<3)
#define STM_OUTPUT_STD_NTSC_M        (1L<<4)
#define STM_OUTPUT_STD_NTSC_J        (1L<<5)
#define STM_OUTPUT_STD_NTSC_443      (1L<<6)
#define STM_OUTPUT_STD_SECAM         (1L<<7)
#define STM_OUTPUT_STD_PAL_60        (1L<<8)
#define STM_OUTPUT_STD_SD_MASK       (STM_OUTPUT_STD_PAL_BDGHI  | \
                                      STM_OUTPUT_STD_PAL_M      | \
                                      STM_OUTPUT_STD_PAL_N      | \
                                      STM_OUTPUT_STD_PAL_Nc     | \
                                      STM_OUTPUT_STD_PAL_60     | \
                                      STM_OUTPUT_STD_NTSC_M     | \
                                      STM_OUTPUT_STD_NTSC_J     | \
                                      STM_OUTPUT_STD_NTSC_443   | \
                                      STM_OUTPUT_STD_SECAM)

#define STM_OUTPUT_STD_SMPTE293M     (1L<<9)
#define STM_OUTPUT_STD_ED_MASK       (STM_OUTPUT_STD_SMPTE293M)

#define STM_OUTPUT_STD_SMPTE240M     (1L<<16)
#define STM_OUTPUT_STD_SMPTE274M     (1L<<17)
#define STM_OUTPUT_STD_SMPTE295M     (1L<<18)
#define STM_OUTPUT_STD_SMPTE296M     (1L<<19)
#define STM_OUTPUT_STD_AS4933        (1L<<20)
#define STM_OUTPUT_STD_HD_MASK       (STM_OUTPUT_STD_SMPTE240M  | \
                                      STM_OUTPUT_STD_SMPTE274M  | \
                                      STM_OUTPUT_STD_SMPTE295M  | \
                                      STM_OUTPUT_STD_SMPTE296M  | \
                                      STM_OUTPUT_STD_AS4933)

#define STM_OUTPUT_STD_VESA          (1L<<24)

#define STM_OUTPUT_STD_TMDS_PIXELREP_2X (1L<<29)
#define STM_OUTPUT_STD_TMDS_PIXELREP_4X (1L<<30)
#define STM_OUTPUT_STD_CEA861C          (1L<<31)

/*
 * Output control capabilities, which controls are currently supported.
 */
#define STM_CTRL_CAPS_BACKGROUND         (1L<<0)
#define STM_CTRL_CAPS_BRIGHTNESS         (1L<<1)
#define STM_CTRL_CAPS_SATURATION         (1L<<2)
#define STM_CTRL_CAPS_CONTRAST           (1L<<3)
#define STM_CTRL_CAPS_HUE                (1L<<4)
#define STM_CTRL_CAPS_DENC_FILTERS       (1L<<5)
#define STM_CTRL_CAPS_CGMS               (1L<<7)
#define STM_CTRL_CAPS_AVMUTE             (1L<<8)
#define STM_CTRL_CAPS_CLOCK_RECOVERY     (1L<<9)
#define STM_CTRL_CAPS_DAC_VOLTAGE        (1L<<10)
#define STM_CTRL_CAPS_AV_SOURCE_SELECT   (1L<<11)
#define STM_CTRL_CAPS_VIDEO_OUT_SELECT   (1L<<12)
#define STM_CTRL_CAPS_625LINE_VBI_23     (1L<<13)
#define STM_CTRL_CAPS_DVO_LOOPBACK       (1L<<14)
#define STM_CTRL_CAPS_GDMA_SETUP         (1L<<15)
#define STM_CTRL_CAPS_SELECT_HW_PLANE    (1L<<16)
#define STM_CTRL_CAPS_SIGNAL_RANGE       (1L<<17)
#define STM_CTRL_CAPS_TELETEXT           (1L<<18)
#define STM_CTRL_CAPS_422_CHROMA_FILTER  (1L<<19)
#define STM_CTRL_CAPS_FDVO_CONTROL       (1L<<20)


typedef enum
{
  STM_CTRL_BRIGHTNESS = 1,              /* STM_CTRL_CAPS_BRIGHTNESS        */
  STM_CTRL_SATURATION,                  /* STM_CTRL_CAPS_SATURATION        */
  STM_CTRL_CONTRAST,                    /* STM_CTRL_CAPS_CONTRAST          */
  STM_CTRL_HUE,                         /* STM_CTRL_CAPS_HUE               */
  STM_CTRL_BACKGROUND_ARGB,             /* STM_CTRL_CAPS_BACKGROUND        */
  STM_CTRL_CVBS_FILTER,                 /* STM_CTRL_CAPS_DENC_FILTERS      */
  STM_CTRL_TELETEXT_SUBTITLES,          /* STM_CTRL_CAPS_625LINE_VBI_23    */
  STM_CTRL_OPEN_SUBTITLES,              /* STM_CTRL_CAPS_625LINE_VBI_23    */
  STM_CTRL_CGMS_ANALOG,                 /* STM_CTRL_CAPS_CGMS              */
  STM_CTRL_PRERECORDED_ANALOGUE_SOURCE, /* STM_CTRL_CAPS_CGMS              */
  STM_CTRL_AVMUTE,                      /* STM_CTRL_CAPS_AVMUTE            */
  STM_CTRL_HDMI_PREAUTH,                /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_POSTAUTH,               /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_USE_HOTPLUG_INTERRUPT,  /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_CEA_MODE_SELECT,        /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_AUDIO_OUT_SELECT,       /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDCP_ADVANCED,               /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_AVI_QUANTIZATION,       /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_OVERSCAN_MODE,          /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_CONTENT_TYPE,           /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_SINK_SUPPORTS_DEEPCOLOUR,/* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_HDMI_PHY_CONF_TABLE,         /* Implied by STM_OUTPUT_CAPS_TMDS */
  STM_CTRL_CLOCK_ADJUSTMENT,            /* STM_CTRL_CAPS_CLOCK_RECOVERY    */
  STM_CTRL_DAC123_MAX_VOLTAGE,          /* STM_CTRL_CAPS_DAC_VOLTAGE       */
  STM_CTRL_DAC123_SATURATION_POINT,     /* STM_CTRL_CAPS_DAC_VOLTAGE       */
  STM_CTRL_DAC456_MAX_VOLTAGE,          /* STM_CTRL_CAPS_DAC_VOLTAGE       */
  STM_CTRL_DAC456_SATURATION_POINT,     /* STM_CTRL_CAPS_DAC_VOLTAGE       */
  STM_CTRL_DAC_HD_POWER,
  STM_CTRL_DAC_HD_FILTER,
  STM_CTRL_AV_SOURCE_SELECT,            /* STM_CTRL_CAPS_AV_SOURCE_SELECT  */
  STM_CTRL_VIDEO_OUT_SELECT,            /* STM_CTRL_CAPS_VIDEO_OUT_SELECT  */
  STM_CTRL_DVO_LOOPBACK,                /* STM_CTRL_CAPS_DVO_LOOPBACK      */
  STM_CTRL_FDVO_INVERT_DATA_CLOCK,      /* STM_CTRL_CAPS_FDVO_CONTROL      */
  STM_CTRL_GDMA_BUFFER_1,               /* STM_CTRL_CAPS_GDMA_SETUP        */
  STM_CTRL_GDMA_BUFFER_2,               /* STM_CTRL_CAPS_GDMA_SETUP        */
  STM_CTRL_GDMA_SIZE,                   /* STM_CTRL_CAPS_GDMA_SETUP        */
  STM_CTRL_GDMA_FORMAT,                 /* STM_CTRL_CAPS_GDMA_SETUP        */
  STM_CTRL_HW_PLANE_ID,                 /* STM_CTRL_CAPS_SELECT_HW_PLANE   */
  STM_CTRL_HW_PLANE_OPACITY,            /* STM_CTRL_CAPS_SELECT_HW_PLANE   */
  STM_CTRL_SIGNAL_RANGE,                /* STM_CTRL_CAPS_SIGNAL_RANGE      */
  STM_CTRL_MAX_PIXEL_CLOCK,      /* Implied by STM_OUTPUT_CAPS_MODE_CHANGE */
  STM_CTRL_TELETEXT_SYSTEM,             /* STM_CTRL_CAPS_TELETEXT          */
  STM_CTRL_422_CHROMA_FILTER,           /* STM_CTRL_CAPS_422_CHROMA_FILTER */
  STM_CTRL_YCBCR_COLORSPACE,     /* Implied by STM_OUTPUT_CAPS_PLANE_MIXER */
  STM_CTRL_SET_MIXER_PLANES
} stm_output_control_t;


#define STM_AV_SOURCE_MAIN_INPUT       (1L<<0)
#define STM_AV_SOURCE_AUX_INPUT        (1L<<1)
#define STM_AV_SOURCE_VIDEOONLY_INPUT  (1L<<2)
#define STM_AV_SOURCE_SPDIF_INPUT      (1L<<3)
#define STM_AV_SOURCE_2CH_I2S_INPUT    (1L<<4)
#define STM_AV_SOURCE_8CH_I2S_INPUT    (1L<<5)

#define STM_VIDEO_OUT_RGB              (1L<<0) /* Analogue, HDMI and multiplexed on 24bits for DVO          */
#define STM_VIDEO_OUT_YUV              (1L<<1) /* Analogue, 4:4:4 on HDMI and multiplexed on 16bits for DVO */
#define STM_VIDEO_OUT_YC               (1L<<2)
#define STM_VIDEO_OUT_CVBS             (1L<<3)
#define STM_VIDEO_OUT_DVI              (1L<<4)
#define STM_VIDEO_OUT_HDMI             (1L<<5)
#define STM_VIDEO_OUT_422              (1L<<7) /* 4:2:2 on HDMI and multiplexed on 16bits for DVO */
#define STM_VIDEO_OUT_ITUR656          (1L<<8) /* 4:2:2 multiplexed on 8bits for DVO              */
#define STM_VIDEO_OUT_YUV_24BIT        (1L<<9) /* 4:4:4 multiplexed on 24bits for DVO             */
#define STM_VIDEO_OUT_HDMI_COLOURDEPTH_SHIFT (30)
#define STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT (0)
#define STM_VIDEO_OUT_HDMI_COLOURDEPTH_30BIT (1L<<STM_VIDEO_OUT_HDMI_COLOURDEPTH_SHIFT)
#define STM_VIDEO_OUT_HDMI_COLOURDEPTH_36BIT (2L<<STM_VIDEO_OUT_HDMI_COLOURDEPTH_SHIFT)
#define STM_VIDEO_OUT_HDMI_COLOURDEPTH_48BIT (3L<<STM_VIDEO_OUT_HDMI_COLOURDEPTH_SHIFT)
#define STM_VIDEO_OUT_HDMI_COLOURDEPTH_MASK  (3L<<STM_VIDEO_OUT_HDMI_COLOURDEPTH_SHIFT)


typedef enum
{
  STM_YCBCR_COLORSPACE_AUTO_SELECT = 0,
  STM_YCBCR_COLORSPACE_601,
  STM_YCBCR_COLORSPACE_709
} stm_ycbcr_colorspace_t;


typedef enum {
  STM_HDMI_CEA_MODE_FOLLOW_PICTURE_ASPECT_RATIO,
  STM_HDMI_CEA_MODE_4_3,
  STM_HDMI_CEA_MODE_16_9
} stm_hdmi_cea_mode_selection_t;


typedef enum
{
  STM_HDMI_AUDIO_TYPE_NORMAL = 0, /* LPCM or IEC61937 compressed audio */
  STM_HDMI_AUDIO_TYPE_ONEBIT,     /* 1-bit audio (SACD)                */
  STM_HDMI_AUDIO_TYPE_DST,        /* Compressed DSD audio streams      */
  STM_HDMI_AUDIO_TYPE_DST_DOUBLE, /* Double Rate DSD audio streams     */
  STM_HDMI_AUDIO_TYPE_HBR,        /* High bit rate compressed audio    */
} stm_hdmi_audio_output_type_t;


typedef enum
{
  STM_HDMI_AVI_QUANTIZATION_UNSUPPORTED = 0,
  STM_HDMI_AVI_QUANTIZATION_RGB  = 1,
  STM_HDMI_AVI_QUANTIZATION_YCC  = 2,
  STM_HDMI_AVI_QUANTIZATION_BOTH = 3,
} stm_hdmi_avi_quantization_t;


typedef enum
{
  STM_SUBTITLES_NONE = 0,
  STM_SUBTITLES_INSIDE_ACTIVE_VIDEO,
  STM_SUBTITLES_OUTSIDE_ACTIVE_VIDEO
} stm_open_subtitles_t;


typedef enum
{
  STM_TELETEXT_SYSTEM_A = 0,
  STM_TELETEXT_SYSTEM_B,
  STM_TELETEXT_SYSTEM_C,
  STM_TELETEXT_SYSTEM_D
} stm_teletext_system_t;


typedef enum
{
  STM_UNKNOWN_FIELD = 0,
  STM_TOP_FIELD,
  STM_BOTTOM_FIELD,
  STM_NOT_A_FIELD /* e.g. a programmable timer */
} stm_field_t;


typedef enum
{
  STM_DISPLAY_DISCONNECTED = 0,
  STM_DISPLAY_CONNECTED,
  STM_DISPLAY_NEEDS_RESTART
} stm_display_status_t;


typedef enum
{
  STM_SIGNAL_FULL_RANGE = 0,
  STM_SIGNAL_FILTER_SAV_EAV,
  STM_SIGNAL_VIDEO_RANGE
} stm_display_signal_range_t;


typedef enum
{
  HDF_COEFF_PHASE1_123,
  HDF_COEFF_PHASE1_456,
  HDF_COEFF_PHASE1_7,
  HDF_COEFF_PHASE2_123,
  HDF_COEFF_PHASE2_456,
  HDF_COEFF_PHASE3_123,
  HDF_COEFF_PHASE3_456,
  HDF_COEFF_PHASE4_123,
  HDF_COEFF_PHASE4_456,
  HDF_COEFF_PHASE_SIZE
} stm_hdf_coeff_phases_t;


typedef enum
{
  HDF_COEFF_2X_LUMA,
  HDF_COEFF_2X_CHROMA,
  HDF_COEFF_2X_ALT_LUMA,
  HDF_COEFF_2X_ALT_CHROMA,
  HDF_COEFF_4X_LUMA,
  HDF_COEFF_4X_CHROMA,
  DENC_COEFF_LUMA,
  DENC_COEFF_CHROMA,
} stm_filter_coeff_types_t;


typedef enum
{
  STM_FLT_DIV_256 = 0,
  STM_FLT_DIV_512,
  STM_FLT_DIV_1024,
  STM_FLT_DIV_2048,
  STM_FLT_DIV_4096,
  STM_FLT_DISABLED
} stm_filter_divide_t;


typedef struct
{
  stm_filter_divide_t      div;
  int                      coeff[10];
} stm_display_denc_filter_setup_t;


typedef struct
{
  stm_filter_divide_t      div;
  ULONG                    coeff[HDF_COEFF_PHASE_SIZE];
} stm_display_hdf_filter_setup_t;


typedef struct
{
  stm_filter_coeff_types_t type;
  union {
    stm_display_denc_filter_setup_t denc;
    stm_display_hdf_filter_setup_t  hdf;
  };
} stm_display_filter_setup_t;


/*
 * Pointer to an array of the following structure is passed to
 * STM_CTRL_HDMI_PHY_CONF_TABLE to provide board and SoC specific
 * configurations of the HDMI PHY. The array should be terminated with an entry
 * that has all fields set to zero.
 */
typedef struct
{
  ULONG minTMDS;   /* Lower bound of TMDS clock this config applies to */
  ULONG maxTMDS;   /* Upper bound of TMDS clock this config applies to */
  ULONG config[4]; /* SoC specific register configuration              */
} stm_display_hdmi_phy_config_t;


struct stm_display_plane_s;
typedef struct stm_display_output_s stm_display_output_t;

typedef struct
{
  int (*GetCapabilities)(stm_display_output_t*, ULONG *caps);

  const stm_mode_line_t* (*GetDisplayMode)(stm_display_output_t*, stm_display_mode_t mode);
  const stm_mode_line_t* (*FindDisplayMode)(stm_display_output_t*, ULONG xres, ULONG yres, ULONG minlines, ULONG minpixels, ULONG pixclock, stm_scan_type_t);

  int (*Start)  (stm_display_output_t*, const stm_mode_line_t*, ULONG tvStandard);
  int (*Stop)   (stm_display_output_t*);

  int (*Suspend)(stm_display_output_t*);
  int (*Resume) (stm_display_output_t*);

  const stm_mode_line_t* (*GetCurrentDisplayMode)  (stm_display_output_t*);
  int                    (*GetCurrentTVStandard)   (stm_display_output_t*, ULONG *standard);

  int (*GetControlCapabilities)(stm_display_output_t*, ULONG *ctrlCaps);
  int (*SetControl)(stm_display_output_t*, stm_output_control_t, ULONG newVal);
  int (*GetControl)(stm_display_output_t*, stm_output_control_t, ULONG *ctrlVal);

  int  (*QueueMetadata)(stm_display_output_t*, stm_meta_data_t*, stm_meta_data_result_t *);
  int  (*FlushMetadata)(stm_display_output_t*, stm_meta_data_type_t);

  int (*SetFilterCoefficients)(stm_display_output_t*, const stm_display_filter_setup_t*);

  int (*EnableBackground)(stm_display_output_t*);
  int (*DisableBackground)(stm_display_output_t*);

  int (*SetClockReference)(stm_display_output_t *, stm_clock_ref_frequency_t, int refClockError);

  void (*SoftReset)(stm_display_output_t*);

  void (*HandleInterrupts)(stm_display_output_t*);

  void (*GetLastVSyncInfo)(stm_display_output_t*, stm_field_t *field, TIME64 *interval);
  void (*GetDisplayStatus)(stm_display_output_t*, stm_display_status_t *status);
  void (*SetDisplayStatus)(stm_display_output_t*, stm_display_status_t status);

  void (*Release)(stm_display_output_t *);

} stm_display_output_ops_t;


struct stm_display_output_s
{
  ULONG handle;
  ULONG lock;

  const struct stm_display_device_s *owner;
  const stm_display_output_ops_t    *ops;
};

/*****************************************************************************
 * C interface for display outputs
 */

/*
 * int stm_display_output_get_capabilities(stm_display_output_t *o, ULONG *caps)
 *
 * Get the OR'd capabilities of the given output, i.e. STM_OUTPUT_CAPS_XXX
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_output_get_capabilities(stm_display_output_t *o, ULONG *caps)
{
  return o->ops->GetCapabilities(o,caps);
}


/*
 * const stm_mode_line_t* stm_display_output_get_display_mode(stm_display_output_t *o,
 *                                                            stm_display_mode_t mode)
 *
 * Get the detailed mode description for the specified mode identifier, if
 * and only if the mode can be displayed on this output.
 *
 * Returns NULL if the device lock cannot be obtained or the specified mode is
 * not supported.
 *
 */
static inline const stm_mode_line_t* stm_display_output_get_display_mode(stm_display_output_t *o, stm_display_mode_t mode)
{
  return o->ops->GetDisplayMode(o,mode);
}


/*
 * const stm_mode_line_t* stm_display_output_find_display_mode(stm_display_output_t *o,
 *                                                             ULONG xres,
 *                                                             ULONG yres,
 *                                                             ULONG minlines,
 *                                                             ULONG minpixels,
 *                                                             ULONG pixclock,
 *                                                             stm_scan_type_t scan)
 *
 * This call searches for a display mode that is supported on the
 * given output, that matches the parameters.
 *
 * 1. xres and yres indicate the active video area required, these must match
 *    exactly.
 * 2. minlines and minpixels refer to the total video lines and total pixels
 *    per line, but these are optional in that a mode matches if its values
 *    are >= to the values specified. Using 0 for these values will match the
 *    first mode that matches the other parameters.
 * 3. pixclock (pixel clock in HZ), this matches with a "fuzz" factor so that
 *    the loss of precision in conversions between clock period (in picoseconds)
 *    to HZ in the Linux framebuffer is ignored.
 * 4. scan, this indicates if the required mode is interlaced or progressive
 *     and must match exactly.
 *
 * Returns NULL if the device lock cannot be obtained or the specified mode is
 * not supported.
 *
 */
static inline const stm_mode_line_t* stm_display_output_find_display_mode(stm_display_output_t *o, ULONG xres, ULONG yres, ULONG minlines, ULONG minpixels, ULONG pixclock, stm_scan_type_t scan)
{
  return o->ops->FindDisplayMode(o,xres,yres,minlines,minpixels,pixclock,scan);
}


/*
 * int stm_display_output_start(stm_display_output_t *o,
 *                              const stm_mode_line_t *mode,
 *                              ULONG tvStandard)
 *
 * Start the given output with the specified display mode. For an output
 * that supports STM_OUTPUT_CAPS_MODE_CHANGE this can be any supported
 * display mode. For outputs that are slaved off a master's timing generator,
 * such as HDMI or DVO, the mode passed to start must be identical
 * to the master's mode (which must already have been started).
 *
 * Returns -1 if the device lock cannot be obtained or if the specified mode
 * cannot be started for any reason, otherwise it returns 0.
 *
 */
static inline int stm_display_output_start(stm_display_output_t *o, const stm_mode_line_t *mode, ULONG tvStandard)
{
  return o->ops->Start(o, mode, tvStandard);
}


/*
 * int stm_display_output_stop(stm_display_output_t *o)
 *
 * Stop the given output such that it no longer produces any display. This
 * will fail if any planes are still connected to the output.
 *
 * Returns -1 if the device lock cannot be obtained or the output cannot
 * currently be stopped, otherwise it returns 0.
 *
 */
static inline int stm_display_output_stop(stm_display_output_t *o)
{
  return o->ops->Stop(o);
}


/*
 * int stm_display_output_suspend(stm_display_output_t *o)
 *
 * Suspend the given output. Used as part of power management operations. It
 * will do whatever is necessary to powerdown the physical outputs and to
 * prevent the hardware accessing memory so that the overall power management
 * system can safely put the DRAM into self refresh. Note that this does
 * not implement hardware state saving to memory for "suspend to disc". It
 * is assumed that the chip continues to be powered sufficiently, to maintain
 * the hardware registers, in the suspended state.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_suspend(stm_display_output_t *o)
{
  return o->ops->Suspend(o);
}


/*
 * int stm_display_output_resume(stm_display_output_t *o)
 *
 * Resume the given output from a suspended state. Used as part of
 * power management operations.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_resume(stm_display_output_t *o)
{
  return o->ops->Resume(o);
}


/*
 * const stm_mode_line_t* stm_display_output_get_current_display_mode(stm_display_output_t *o)
 *
 * Query the current display mode running on the given output
 *
 * Return NULL if the device lock cannot be obtained  or if the output is
 * currently stopped.
 *
 */
static inline const stm_mode_line_t* stm_display_output_get_current_display_mode(stm_display_output_t *o)
{
  return o->ops->GetCurrentDisplayMode(o);
}


/*
 * int stm_display_output_get_current_tv_standard(stm_display_output_t *o,
 *                                                ULONG *standard)
 *
 * Query the current TV standard. This is only really relevant for SD
 * interlaced modes which can have different encodings, e.g. PAL/SECAM,
 * NTSC/NTSC-J etc..  If the output is stopped then the standard will be
 * set to zero.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_get_current_tv_standard(stm_display_output_t *o, ULONG *standard)
{
  return o->ops->GetCurrentTVStandard(o,standard);
}


/*
 * int stm_display_output_get_control_capabilities(stm_display_output_t *o,
 *                                                 ULONG *ctrlCaps)
 *
 * Return the or'd control capabilities that the given output supports,
 * i.e. STM_CTRL_CAPS_XXXX
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_output_get_control_capabilities(stm_display_output_t *o, ULONG *ctrlCaps)
{
  return o->ops->GetControlCapabilities(o,ctrlCaps);
}


/*
 * int stm_display_output_set_control(stm_display_output_t *o,
 *                                    stm_output_control_t ctrl,
 *                                    ULONG newVal)
 *
 * Set the specified control to the given value. Note that if the control is
 * not supported by the output then the request is simply ignored, i.e. that
 * is not considered an error. If an attempt to set an invalid value to an
 * existing control is made, the request is again ignored and no error returned.
 * If it is important to know if the control value has actually been updated,
 * you should subsequently call stm_display_output_get_control().
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_output_set_control(stm_display_output_t *o, stm_output_control_t ctrl, ULONG newVal)
{
  return o->ops->SetControl(o, ctrl, newVal);
}


/*
 * int stm_display_output_get_control(stm_display_output_t *o,
 *                                    stm_output_control_t ctrl,
 *                                    ULONG *ctrlVal)
 *
 * Get the value of the specified control. Note that if the control is
 * not supported by the output then ctrlVal will be set to 0, i.e. that
 * is not considered an error.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_get_control(stm_display_output_t *o, stm_output_control_t ctrl, ULONG *ctrlVal)
{
  return o->ops->GetControl(o, ctrl, ctrlVal);
}


/*
 * int stm_display_output_queue_metadata(stm_display_output_t *o,
 *                                       stm_meta_data_t *d,
 *                                       stm_meta_data_result_t *r)
 *
 * Queue the metadata structure d on the output for presentation. For more
 * details see include/stmmetadata.h
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 * The result of the operation is returned in r.
 */
static inline int stm_display_output_queue_metadata(stm_display_output_t *o, stm_meta_data_t *d, stm_meta_data_result_t *r)
{
  return o->ops->QueueMetadata(o, d, r);
}


/*
 * int stm_display_output_flush_metadata(stm_display_output_t *o,
 *                                       stm_meta_data_type_t t)
 *
 * Flush all entries of the specified metadata type t from the output's queue.
 * If t is zero then all types supported by the output are flushed.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_flush_metadata(stm_display_output_t *o, stm_meta_data_type_t t)
{
  return o->ops->FlushMetadata(o, t);
}


/*
 * int stm_display_output_set_filter_coefficients(stm_display_output_t *o,
 *                                                const stm_display_filter_setup_t *f);
 *
 * Change the filter coefficents defined by f.
 *
 * Returns -1 if the device lock cannot be obtained (the caller should check
 * for pending signals to identify this condition) or if the output does not
 * support the specified filter set, otherwise it returns 0.
 *
 */
static inline int stm_display_output_set_filter_coefficients(stm_display_output_t *o, const stm_display_filter_setup_t *f)
{
  return o->ops->SetFilterCoefficients(o, f);
}


/*
 * int stm_display_output_enable_background(stm_display_output_t *o)
 *
 * Enable the output's mixer background, if it has one. The default state
 * of the output's background is implementation dependent.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_enable_background(stm_display_output_t *o)
{
  return o->ops->EnableBackground(o);
}


/*
 * int stm_display_output_disable_background(stm_display_output_t *o)
 *
 * Disable the output's mixer background, if it has one. The default state
 * of the output's background is implementation dependent.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 */
static inline int stm_display_output_disable_background(stm_display_output_t *o)
{
  return o->ops->DisableBackground(o);
}


/*
 * void stm_display_output_handle_interrupts(stm_display_output_t *o)
 *
 * Called by first level interrupt handlers to handle all hardware interrupts
 * associated with the output. For master outputs that support mode change
 * this will at least include the associated VSync interrupt. However it may
 * include other interrupts, such as those required by HDMI.
 */
static inline void stm_display_output_handle_interrupts(stm_display_output_t *o)
{
  o->ops->HandleInterrupts(o);
}


/*
 * void stm_display_output_get_last_vsync_info(stm_display_output_t *o,
 *                                             stm_field_t *field,
 *                                             TIME64 *interval)
 *
 * Get Information about the last VSync for the output, i.e. top/bottom field
 * in interlaced modes and the time interval between the last two VSyncs. This
 * can be called from interrupt context, but as such does not take any locks
 * or return errors.
 *
 */
static inline void stm_display_output_get_last_vsync_info(stm_display_output_t *o, stm_field_t *field, TIME64 *interval)
{
  o->ops->GetLastVSyncInfo(o,field,interval);
}


/*
 * void stm_display_output_get_status(stm_display_output_t *o,
 *                                    stm_display_status_t *status)
 *
 * Get the status of the output to the physical display; analogue outputs
 * always return STM_DISPLAY_CONNECTED as there is no way of telling if
 * a display is connected or not. An HDMI output will change its state based
 * on changes to the hot plug pin on the HDMI connector; when that is connected
 * to the HDMI block's interrupts in the chip. This can be called
 * from interrupt context, therefore it does not take any locks or return
 * errors.
 */
static inline void stm_display_output_get_status(stm_display_output_t *o, stm_display_status_t *status)
{
  o->ops->GetDisplayStatus(o,status);
}


/*
 * void stm_display_output_set_status(stm_display_output_t *o,
 *                                    stm_display_status_t status)
 *
 * Set the status of the output to the physical display. Used to inform
 * an output about a platform specific change to the output status such as pio
 * polled hdmi hotplug events. This can be called from interrupt context,
 * therefore it does not take any locks or return errors.
 */
static inline void stm_display_output_set_status(stm_display_output_t *o, stm_display_status_t status)
{
  o->ops->SetDisplayStatus(o,status);
}


/*
 * int stm_display_output_setup_clock_reference(stm_display_output_t     *o,
 *                                              stm_clock_ref_frequency_t refClock,
 *                                              int                       refClockError)
 *
 * Change the reference clock configuration for the output's fsynth from the
 * default values for the SoC.
 */
static inline int stm_display_output_setup_clock_reference(stm_display_output_t *o,stm_clock_ref_frequency_t refClock, int refClockError)
{
  return o->ops->SetClockReference(o,refClock,refClockError);
}


/*
 * int stm_display_output_soft_reset(stm_display_output_t *o)
 *
 * Issue an output specific "soft reset". This is currently defined for
 * master outputs that own a video timing generator, where the timing generator
 * will be reset and will immediately issue a top field vsync.
 *
 * The main use of this is to do a hard realignment of the output vsync to be
 * as close to an input vsync on the digital video capture hardware as software
 * latencies allow. It is likely to be called in the interrupt handler of the
 * input driver, hence this call does not take any locks or return errors.
 */
static inline void stm_display_output_soft_reset(stm_display_output_t *o)
{
  return o->ops->SoftReset(o);
}


/*
 * void stm_display_output_release(stm_display_output_t *o)
 *
 * Release the given output instance. The instance pointer is invalid after
 * this call completes. Releasing the instance has no effect on the underlying
 * hardware, that is if the output is running it will continue to do so until
 * expicitly stopped.
 *
 */
static inline void stm_display_output_release(stm_display_output_t *o)
{
  o->ops->Release(o);
}

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_OUTPUT_H */
