/***********************************************************************
 *
 * File: include/stm_display_modes.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_MODES_H
#define _STM_DISPLAY_MODES_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_modes.h
 *  \brief Definitions, types and structures used to describe display modes
 */

/*! \enum  stm_display_mode_id_e
 *  \brief Display timing mode identifiers
 *  \apis  ::stm_display_output_get_display_mode()
 */
typedef enum stm_display_mode_id_e
{
  STM_TIMING_MODE_RESERVED = 0,
  STM_TIMING_MODE_480P60000_27027,       /*!< NTSC P60 */
  STM_TIMING_MODE_480I59940_13500,       /*!< NTSC */
  STM_TIMING_MODE_480P59940_27000,       /*!< NTSC P59.94 (525p ?) */
  STM_TIMING_MODE_480I59940_12273,       /*!< NTSC square, PAL M square */
  STM_TIMING_MODE_576I50000_13500,       /*!< PAL B,D,G,H,I,N, SECAM */
  STM_TIMING_MODE_576P50000_27000,       /*!< PAL P50 (625p ?)*/
  STM_TIMING_MODE_576I50000_14750,       /*!< PAL B,D,G,H,I,N, SECAM square */

  STM_TIMING_MODE_1080P60000_148500,     /*!< SMPTE 274M #1  P60 */
  STM_TIMING_MODE_1080P59940_148352,     /*!< SMPTE 274M #2  P60 /1.001 */
  STM_TIMING_MODE_1080P50000_148500,     /*!< SMPTE 274M #3  P50 */
  STM_TIMING_MODE_1080P30000_74250,      /*!< SMPTE 274M #7  P30 */
  STM_TIMING_MODE_1080P29970_74176,      /*!< SMPTE 274M #8  P30 /1.001 */
  STM_TIMING_MODE_1080P25000_74250,      /*!< SMPTE 274M #9  P25 */
  STM_TIMING_MODE_1080P24000_74250,      /*!< SMPTE 274M #10 P24 */
  STM_TIMING_MODE_1080P23976_74176,      /*!< SMPTE 274M #11 P24 /1.001 */

  STM_TIMING_MODE_1080I60000_74250,      /*!< EIA770.3 #3 I60 = SMPTE274M #4 I60 */
  STM_TIMING_MODE_1080I59940_74176,      /*!< EIA770.3 #4 I60 /1.001 = SMPTE274M #5 I60 /1.001 */
  STM_TIMING_MODE_1080I50000_74250_274M, /*!< SMPTE 274M Styled 1920x1080 I50 CEA/HDMI Code 20 */
  STM_TIMING_MODE_1080I50000_72000,      /*!< AS 4933.1-2005 1920x1080 I50 CEA/HDMI Code 39 */

  STM_TIMING_MODE_720P60000_74250,       /*!< EIA770.3 #1 P60 = SMPTE 296M #1 P60 */
  STM_TIMING_MODE_720P59940_74176,       /*!< EIA770.3 #2 P60 /1.001= SMPTE 296M #2 P60 /1.001 */
  STM_TIMING_MODE_720P50000_74250,       /*!< SMPTE 296M Styled 1280x720 50P CEA/HDMI Code 19 */

  STM_TIMING_MODE_1152I50000_48000,      /*!< AS 4933.1 1280x1152 I50 */

  STM_TIMING_MODE_480P59940_25180,       /*!< 640x480p @ 59.94Hz                    */
  STM_TIMING_MODE_480P60000_25200,       /*!< 640x480p @ 60Hz                       */
  STM_TIMING_MODE_768P60000_65000,       /*!< 1024*768P XGA mode @ 60Hz             */
  STM_TIMING_MODE_768P70000_75000,       /*!< 1024*768P XGA mode @ 70Hz            */
  STM_TIMING_MODE_768P75000_78750,       /*!< 1024*768P XGA mode @ 75Hz            */
  STM_TIMING_MODE_768P85000_94500,       /*!< 1024*768P XGA mode @ 85Hz            */
  STM_TIMING_MODE_1024P60000_108000,     /*!< 1280*1024P SXGA mode @ 60Hz           */

  STM_TIMING_MODE_540P29970_18000,       /*!< NTG5: QFHD1830,  Quarter HD 30Hz */
  STM_TIMING_MODE_540P25000_18000,       /*!< NTG5: QFHD1825,  Quarter HD 25Hz */
  STM_TIMING_MODE_540P59940_36343,       /*!< NTG5: QFHD3660,  Quarter HD 60Hz */
  STM_TIMING_MODE_540P49993_36343,       /*!< NTG5: QFHD3650,  Quarter HD 50Hz */
  STM_TIMING_MODE_540P59945_54857_WIDE,  /*!< NTG5: WQFHD5660, 1440x540 60Hz   */
  STM_TIMING_MODE_540P49999_54857_WIDE,  /*!< NTG5: WQFHD5650, 1440x540 50Hz   */
  STM_TIMING_MODE_540P59945_54857,       /*!< NTG5: QFHD5660,  Quarter HD 60Hz */
  STM_TIMING_MODE_540P49999_54857,       /*!< NTG5: QFHD5650,  Quarter HD 50Hz */

  STM_TIMING_MODE_480I59940_27000,       /*!< 480i @ 59.94Hz pixel doubled for HDMI */
  STM_TIMING_MODE_576I50000_27000,       /*!< 576i @ 50Hz pixel doubled for HDMI    */
  STM_TIMING_MODE_480I59940_54000,       /*!< 480i @ 59.94Hz pixel quaded for HDMI  */
  STM_TIMING_MODE_576I50000_54000,       /*!< 576i @ 50Hz pixel quaded for HDMI     */
  STM_TIMING_MODE_480P59940_54000,       /*!< 480p @ 59.94Hz pixel doubled for HDMI */
  STM_TIMING_MODE_480P60000_54054,       /*!< 480p @ 60Hz pixel doubled for HDMI    */
  STM_TIMING_MODE_576P50000_54000,       /*!< 576p @ 50Hz pixel doubled for HDMI    */
  STM_TIMING_MODE_480P59940_108000,      /*!< 480p @ 59.94Hz pixel quaded for HDMI  */
  STM_TIMING_MODE_480P60000_108108,      /*!< 480p @ 60Hz pixel doubled for HDMI    */
  STM_TIMING_MODE_576P50000_108000,      /*!< 576p @ 50Hz pixel quaded for HDMI     */

  STM_TIMING_MODE_1080P119880_296703,    /*!< CEA/HDMI code 63 */
  STM_TIMING_MODE_1080P120000_297000,    /*!< CEA/HDMI code 63 */
  STM_TIMING_MODE_1080P100000_297000,    /*!< CEA/HDMI code 64 */

  STM_TIMING_MODE_4K2K29970_296703,      /*!< HDMI extended resolution VIC 1 */
  STM_TIMING_MODE_4K2K30000_297000,      /*!< HDMI extended resolution VIC 1 */
  STM_TIMING_MODE_4K2K25000_297000,      /*!< HDMI extended resolution VIC 2 */
  STM_TIMING_MODE_4K2K23980_296703,      /*!< HDMI extended resolution VIC 3 */
  STM_TIMING_MODE_4K2K24000_297000,      /*!< HDMI extended resolution VIC 3 */
  STM_TIMING_MODE_4K2K24000_297000_WIDE, /*!< HDMI extended resolution VIC 4 */

  STM_TIMING_MODE_COUNT,                         /*!< must stay last */
  STM_TIMING_MODE_CUSTOM = STM_TIMING_MODE_COUNT /*!< Alias for custom modes */
} stm_display_mode_id_t;


/*! \enum stm_scan_type_e
 *  \brief Scan mode, progressive or interlaced
 *
 *  Used in a display mode description defined in ::stm_display_mode_parameters_s
 */
typedef enum stm_scan_type_e
{
  STM_PROGRESSIVE_SCAN = 1, /*!< */
  STM_INTERLACED_SCAN = 2   /*!< */
} stm_scan_type_t;


/*! \enum stm_sync_polarity_e
 *  \brief H and V sync polarity
 *
 *  Used in a display mode description defined in ::stm_display_mode_parameters_s
 */
typedef enum stm_sync_polarity_e
{
  STM_SYNC_NEGATIVE, /*!< */
  STM_SYNC_POSITIVE  /*!< */
} stm_sync_polarity_t;


/*! \struct stm_display_mode_timing_s
 *  \brief Display mode timing parameters
 *
 *  Used in a display mode description defined in ::stm_display_mode_s
 */
typedef struct stm_display_mode_timing_s
{
  uint32_t            pixels_per_line;    /*!< Total number of pixels per line
                                             including the blanking area */
  uint32_t            lines_per_frame;    /*!< Total number of lines per progressive
                                             frame or pair of interlaced fields
                                             including the blanking area */
  uint32_t            pixel_clock_freq;   /*!< In Hz, e.g. 27000000 (27MHz) */
  stm_sync_polarity_t hsync_polarity;     /*!< */
  uint32_t            hsync_width;        /*!< */
  stm_sync_polarity_t vsync_polarity;     /*!< */
  uint32_t            vsync_width;        /*!< */
} stm_display_mode_timing_t;


/*! \enum stm_mode_aspect_ratio_index_e
 *  \brief Indexing into aspect ratio specific information in ::stm_display_mode_parameters_s
 */
typedef enum stm_mode_aspect_ratio_index_e
{
  STM_AR_INDEX_4_3 = 0,  /*!< */
  STM_AR_INDEX_16_9,     /*!< */
  STM_AR_INDEX_64_27,    /*!< */
  STM_AR_INDEX_COUNT     /*!< */
} stm_mode_aspect_ratio_index_t;


/*! \enum stm_output_standards_e
 *  \brief Output standard bitmask defines used in ::stm_display_mode_parameters_s
 */
typedef enum stm_output_standards_e
{
  STM_OUTPUT_STD_UNDEFINED        = 0,       /*!< */
  STM_OUTPUT_STD_PAL_BDGHI        = (1L<<0), /*!< */
  STM_OUTPUT_STD_PAL_M            = (1L<<1), /*!< */
  STM_OUTPUT_STD_PAL_N            = (1L<<2), /*!< */
  STM_OUTPUT_STD_PAL_Nc           = (1L<<3), /*!< */
  STM_OUTPUT_STD_NTSC_M           = (1L<<4), /*!< */
  STM_OUTPUT_STD_NTSC_J           = (1L<<5), /*!< */
  STM_OUTPUT_STD_NTSC_443         = (1L<<6), /*!< */
  STM_OUTPUT_STD_SECAM            = (1L<<7), /*!< */
  STM_OUTPUT_STD_PAL_60           = (1L<<8), /*!< */
  STM_OUTPUT_STD_SD_MASK          = (STM_OUTPUT_STD_PAL_BDGHI  | \
                                     STM_OUTPUT_STD_PAL_M      | \
                                     STM_OUTPUT_STD_PAL_N      | \
                                     STM_OUTPUT_STD_PAL_Nc     | \
                                     STM_OUTPUT_STD_PAL_60     | \
                                     STM_OUTPUT_STD_NTSC_M     | \
                                     STM_OUTPUT_STD_NTSC_J     | \
                                     STM_OUTPUT_STD_NTSC_443   | \
                                     STM_OUTPUT_STD_SECAM), /*!< */

  STM_OUTPUT_STD_SMPTE293M        = (1L<<9),  /*!< */
  STM_OUTPUT_STD_ITUR1358         = (1L<<10), /*!< */
  STM_OUTPUT_STD_ED_MASK          = (STM_OUTPUT_STD_SMPTE293M |
                                     STM_OUTPUT_STD_ITUR1358), /*!< */

  STM_OUTPUT_STD_SMPTE240M        = (1L<<11), /*!< */
  STM_OUTPUT_STD_SMPTE274M        = (1L<<12), /*!< */
  STM_OUTPUT_STD_SMPTE295M        = (1L<<13), /*!< */
  STM_OUTPUT_STD_SMPTE296M        = (1L<<14), /*!< */
  STM_OUTPUT_STD_AS4933           = (1L<<15), /*!< */
  STM_OUTPUT_STD_HD_MASK          = (STM_OUTPUT_STD_SMPTE240M  | \
                                     STM_OUTPUT_STD_SMPTE274M  | \
                                     STM_OUTPUT_STD_SMPTE295M  | \
                                     STM_OUTPUT_STD_SMPTE296M  | \
                                     STM_OUTPUT_STD_AS4933), /*!< */

  STM_OUTPUT_STD_VGA              = (1L<<22), /*!< */
  STM_OUTPUT_STD_XGA              = (1L<<23), /*!< */
  STM_OUTPUT_STD_VESA_MASK        = (STM_OUTPUT_STD_VGA | \
                                     STM_OUTPUT_STD_XGA), /*!< */

  STM_OUTPUT_STD_NTG5             = (1L<<25), /*!< */
  STM_OUTPUT_STD_CEA861           = (1L<<26), /*!< */
  STM_OUTPUT_STD_HDMI_LLC_EXT     = (1L<<27), /*!< */
} stm_output_standards_t;


/*! \enum stm_display_mode_flags_e
 *  \brief Display mode modifier flag defines used in ::stm_display_mode_parameters_s
 */
typedef enum stm_display_mode_flags_e
{
  STM_MODE_FLAGS_NONE                 = 0,       /*!< */
  STM_MODE_FLAGS_3D_FRAME_PACKED      = (1L<<0), /*!< */
  STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE = (1L<<1), /*!< */
  STM_MODE_FLAGS_3D_SBS_HALF          = (1L<<2), /*!< */
  STM_MODE_FLAGS_3D_TOP_BOTTOM        = (1L<<3), /*!< */
  STM_MODE_FLAGS_3D_FRAME_SEQUENTIAL  = (1L<<4), /*!< */
  STM_MODE_FLAGS_3D_LL_RR             = (1L<<5), /*!< */
  STM_MODE_FLAGS_3D_LINE_ALTERNATIVE  = (1L<<6), /*!< */
  STM_MODE_FLAGS_3D_MASK              = (STM_MODE_FLAGS_3D_FRAME_PACKED      |
                                         STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE |
                                         STM_MODE_FLAGS_3D_SBS_HALF          |
                                         STM_MODE_FLAGS_3D_TOP_BOTTOM        |
                                         STM_MODE_FLAGS_3D_FRAME_SEQUENTIAL  |
                                         STM_MODE_FLAGS_3D_LL_RR             |
                                         STM_MODE_FLAGS_3D_LINE_ALTERNATIVE), /*!< */

  STM_MODE_FLAGS_HDMI_PIXELREP_2X     = (1L<<16), /*!< */
  STM_MODE_FLAGS_HDMI_PIXELREP_4X     = (1L<<17), /*!< */
  STM_MODE_FLAGS_HDMI_PIXELREP_MASK   = (STM_MODE_FLAGS_HDMI_PIXELREP_2X |
                                         STM_MODE_FLAGS_HDMI_PIXELREP_4X) /*!< */
} stm_display_mode_flags_t;


/*! \struct stm_display_mode_parameters_s
 *  \brief Display mode parameters
 */
typedef struct stm_display_mode_parameters_s
{
  uint32_t        vertical_refresh_rate;   /*!< Vertical refresh rate in mHz,
                                             e.g. 60000 == 60Hz*/
  stm_scan_type_t scan_type;               /*!< */
  uint32_t        active_area_width;       /*!< */
  uint32_t        active_area_height;      /*!< */
  uint32_t        active_area_start_pixel; /*!< Pixels are counted from 0 */
  uint32_t        active_area_start_line;  /*!< Lines are counted from 1, starting
                                             with the first vsync blanking line */
  uint32_t        output_standards;        /*!< OR'd ::stm_output_standards_e values
                                             supported by this mode */
  stm_rational_t  pixel_aspect_ratios[STM_AR_INDEX_COUNT]; /*!< */
  uint32_t        hdmi_vic_codes[STM_AR_INDEX_COUNT];      /*!< */
  uint32_t        flags;                   /*!< OR'd ::stm_display_mode_flags_e
                                             values supported by this mode */
} stm_display_mode_parameters_t;


/*! \struct stm_display_mode_s
 *  \brief A complete display mode description
 *  \apis ::stm_display_output_get_display_mode(),
 *        ::stm_display_output_find_display_mode(),
 *        ::stm_display_output_get_current_display_mode(),
 *        ::stm_display_output_start()
 */
typedef struct stm_display_mode_s
{
  stm_display_mode_id_t           mode_id;      /*!< */
  stm_display_mode_parameters_t   mode_params;  /*!< */
  stm_display_mode_timing_t       mode_timing;  /*!< */
} stm_display_mode_t;

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_MODES_H */
