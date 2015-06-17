/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2000-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display_device_priv.h>

#include "DisplayDevice.h"
#include "Output.h"
#include "DisplayPlane.h"

/* {mode_id},
 * {vertical_refresh_rate, scan_type, active_area_width, active_area_height, active_area_start_pixel, active_area_start_line, output_standards, SquarePixel, HDMIVideoCode},
 * {pixels_per_line, lines_per_frame, pixel_clock_freq, hsync_polarity, hsync_width, vsync_polarity, vsync_width}
 *
 * Note that active_area_height is given in frame lines even for interlaced modes,
 * so the actual number of lines per interlaced field is half the given numbers.
 *
 * active_area_start_line is specified as the frame or odd field line number,
 * starting from 1 which is the first vsync pulse line for all modes
 * including 525line modes.
 *
 * vsync_width is specified in the number of lines to be generated per
 * progressive frame or interlaced field. The SD lengths are for digital
 * blanking, the DENC generates the correct VBI signals for analogue signals.
 * The 576i (625line) timing uses the 3 line system described in CEA-861-C,
 * rather than 2.5 lines.
 *
 * SD Progressive modes describe a SMPTE293M system.
 *
 * The active_area_start_line for non-square-pixel 480i (525line) modes is based
 * on the CEA-861-C definition of 480i for digital interfaces and therefore the
 * video starts one field line early compared with the original 525line analog
 * standards. Note that this does not effect line 20/21 data services on
 * analog outputs.
 *
 * The horizontal blanking for non-square-pixel 480i modes is also based on
 * the digital CEA-861-C definition and as a result the analog output of
 * these modes will have the image shifted by ~4pixels.
 *
 * Interestingly the definitions of the analog and digital versions of
 * 576i (625line) modes are identical. Go figure?
 *
 */

#define PIX_AR_NULL   {0,0}
#define PIX_AR_SQUARE {1,1}
#define PIX_AR_8_9    {8,9}
#define PIX_AR_32_27  {32,27}
#define PIX_AR_16_15  {16,15}
#define PIX_AR_64_45  {64,45}
#define PIX_AR_4_3    {4,3}

#define STM_480I_5994_STANDARDS (STM_OUTPUT_STD_NTSC_M   | \
                                 STM_OUTPUT_STD_NTSC_443 | \
                                 STM_OUTPUT_STD_NTSC_J   | \
                                 STM_OUTPUT_STD_PAL_M    | \
                                 STM_OUTPUT_STD_PAL_60)

#define STM_576I_50_STANDARDS   (STM_OUTPUT_STD_PAL_BDGHI | \
                                 STM_OUTPUT_STD_PAL_N     | \
                                 STM_OUTPUT_STD_PAL_Nc    | \
                                 STM_OUTPUT_STD_SECAM)


static const stm_display_mode_t ModeParamsTable[] =
{
  { STM_TIMING_MODE_RESERVED },
  /* SD/ED modes */
  { STM_TIMING_MODE_480P60000_27027,
    { 60000, STM_PROGRESSIVE_SCAN, 720, 480, 122, 37, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE293M), {PIX_AR_8_9, PIX_AR_32_27, PIX_AR_NULL}, {2,3,0} },
    { 858, 525, 27027000, STM_SYNC_NEGATIVE, 62, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_480I59940_13500,
    { 59940, STM_INTERLACED_SCAN, 720, 480, 119, 19, STM_480I_5994_STANDARDS, {PIX_AR_8_9, PIX_AR_32_27, PIX_AR_NULL}, {0,0,0} },
    { 858, 525, 13500000, STM_SYNC_NEGATIVE, 62, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_480P59940_27000,
    { 59940, STM_PROGRESSIVE_SCAN, 720, 480, 122, 37, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE293M), {PIX_AR_8_9, PIX_AR_32_27, PIX_AR_NULL}, {2,3,0} },
    { 858, 525, 27000000, STM_SYNC_NEGATIVE, 62, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_480I59940_12273,
    { 59940, STM_INTERLACED_SCAN, 640, 480, 118, 20, (STM_OUTPUT_STD_NTSC_M | STM_OUTPUT_STD_NTSC_J | STM_OUTPUT_STD_PAL_M), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0} },
    { 780, 525, 12272727, STM_SYNC_NEGATIVE, 59, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_576I50000_13500,
    { 50000, STM_INTERLACED_SCAN, 720, 576, 132, 23, STM_576I_50_STANDARDS, {PIX_AR_16_15, PIX_AR_64_45, PIX_AR_NULL}, {0,0,0} },
    { 864, 625, 13500000, STM_SYNC_NEGATIVE, 63, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_576P50000_27000,
    { 50000, STM_PROGRESSIVE_SCAN, 720, 576, 132, 45, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE293M), {PIX_AR_16_15, PIX_AR_64_45, PIX_AR_NULL}, {17,18,0} },
    { 864, 625, 27000000, STM_SYNC_NEGATIVE, 64, STM_SYNC_NEGATIVE, 5}
  },
  { STM_TIMING_MODE_576I50000_14750,
    { 50000, STM_INTERLACED_SCAN, 768, 576, 155, 23, STM_576I_50_STANDARDS, {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0} },
    { 944, 625, 14750000, STM_SYNC_NEGATIVE, 71, STM_SYNC_NEGATIVE, 3}
  },

  /*
   * 1080p modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { STM_TIMING_MODE_1080P60000_148500,
    { 60000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,16,76} },
    { 2200, 1125, 148500000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P59940_148352,
    { 59940, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,16,76} },
    { 2200, 1125, 148351648, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P50000_148500,
    { 50000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,31,75} },
    { 2640, 1125, 148500000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P30000_74250,
    { 30000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,34,74} },
    { 2200, 1125,  74250000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P29970_74176,
    { 29970, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,34,74} },
    { 2200, 1125,  74175824, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P25000_74250,
    { 25000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,33,73} },
    { 2640, 1125,  74250000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P24000_74250,
    { 24000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,32,72} },
    { 2750, 1125,  74250000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P23976_74176,
    { 23976, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,32,72} },
    { 2750, 1125,  74175824, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },

  /*
   * 1080i modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { STM_TIMING_MODE_1080I60000_74250,
    { 60000, STM_INTERLACED_SCAN, 1920, 1080, 192, 21, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,5,0} },
    { 2200, 1125,  74250000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080I59940_74176,
    { 59940, STM_INTERLACED_SCAN, 1920, 1080, 192, 21, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,5,0} },
    { 2200, 1125,  74175824, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080I50000_74250_274M,
    { 50000, STM_INTERLACED_SCAN, 1920, 1080, 192, 21, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,20,0} },
    { 2640, 1125,  74250000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  /* Australian 1080i. */
  { STM_TIMING_MODE_1080I50000_72000,
    { 50000, STM_INTERLACED_SCAN, 1920, 1080, 352, 63, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_AS4933), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,39,0} },
    { 2304, 1250,  72000000, STM_SYNC_POSITIVE, 168, STM_SYNC_NEGATIVE,  5}
  },

  /*
   * 720p modes, SMPTE 296M (analogue) and CEA-861-C (HDMI)
   */
  { STM_TIMING_MODE_720P60000_74250,
    { 60000, STM_PROGRESSIVE_SCAN, 1280,  720, 260, 26, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE296M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,4,69} },
    { 1650,  750,  74250000, STM_SYNC_POSITIVE, 40, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_720P59940_74176,
    { 59940, STM_PROGRESSIVE_SCAN, 1280,  720, 260, 26, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE296M), {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,4,69} },
    { 1650,  750,  74175824, STM_SYNC_POSITIVE, 40, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_720P50000_74250,
    { 50000, STM_PROGRESSIVE_SCAN, 1280,  720, 260, 26, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_SMPTE296M), {PIX_AR_NULL, PIX_AR_SQUARE,PIX_AR_4_3}, {0,19,68} },
    { 1980,  750,  74250000, STM_SYNC_POSITIVE,  40, STM_SYNC_POSITIVE, 5}
  },

  /*
   * A 1280x1152@50Hz Australian analogue HD mode.
   */
  { STM_TIMING_MODE_1152I50000_48000,
    { 50000, STM_INTERLACED_SCAN, 1280, 1152, 235, 90, STM_OUTPUT_STD_AS4933, {PIX_AR_NULL, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0} },
    { 1536, 1250,  48000000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },

  /*
   * good old VGA, the default fallback mode for HDMI displays, note that
   * the same video code is used for 4x3 and 16x9 displays and it is up to
   * the display to determine how to present it (see CEA-861-C). Also note
   * that CEA specifies the pixel aspect ratio is 1:1 .
   */
  { STM_TIMING_MODE_480P59940_25180,
    { 59940, STM_PROGRESSIVE_SCAN, 640, 480, 144, 36, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_VGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {1,0,0} },
    { 800, 525, 25174800, STM_SYNC_NEGATIVE, 96, STM_SYNC_NEGATIVE, 2}
  },
  { STM_TIMING_MODE_480P60000_25200,
    { 60000, STM_PROGRESSIVE_SCAN, 640, 480, 144, 36, (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_VGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {1,0,0} },
    { 800, 525, 25200000, STM_SYNC_NEGATIVE, 96, STM_SYNC_NEGATIVE, 2}
  },

  /*1024*768P XGA modes */
  { STM_TIMING_MODE_768P60000_65000,
    {60000, STM_PROGRESSIVE_SCAN, 1024, 768, 296, 36, (STM_OUTPUT_STD_XGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0}},
    { 1344, 806, 65000000, STM_SYNC_NEGATIVE, 136, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_768P70000_75000,
    {70000, STM_PROGRESSIVE_SCAN, 1024, 768, 280, 36, (STM_OUTPUT_STD_XGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0}},
    { 1328, 806, 75000000, STM_SYNC_NEGATIVE, 136, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_768P75000_78750,
    {75000, STM_PROGRESSIVE_SCAN, 1024, 768, 272, 32, (STM_OUTPUT_STD_XGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0}},
    { 1312, 800, 78750000, STM_SYNC_POSITIVE, 96, STM_SYNC_POSITIVE, 3}
  },
  { STM_TIMING_MODE_768P85000_94500,
    {85000, STM_PROGRESSIVE_SCAN, 1024, 768, 304, 40, (STM_OUTPUT_STD_XGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0}},
    { 1376, 808, 94500000, STM_SYNC_POSITIVE, 96, STM_SYNC_POSITIVE, 3}
  },

  /*1280 x 1024 modes */
  { STM_TIMING_MODE_1024P60000_108000,
    {60000, STM_PROGRESSIVE_SCAN, 1280, 1024, 360, 42, (STM_OUTPUT_STD_XGA), {PIX_AR_SQUARE, PIX_AR_NULL, PIX_AR_NULL}, {0,0,0}},
    { 1688, 1066, 108000000, STM_SYNC_POSITIVE, 112, STM_SYNC_POSITIVE, 3}
  },
  /*
   * NTG5 Automotive display modes
   */
  { STM_TIMING_MODE_540P29970_18000,
    { 29970, STM_PROGRESSIVE_SCAN, 960, 540, 30, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1001, 600, 18000000, STM_SYNC_POSITIVE, 20, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P25000_18000,
    { 25000, STM_PROGRESSIVE_SCAN, 960, 540, 30, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1200, 600, 18000000, STM_SYNC_POSITIVE, 20, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P59940_36343,
    { 59940, STM_PROGRESSIVE_SCAN, 960, 540, 60, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1040, 583, 36343000, STM_SYNC_POSITIVE, 40, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P49993_36343,
    { 49993, STM_PROGRESSIVE_SCAN, 960, 540, 60, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1040, 699, 36343000, STM_SYNC_POSITIVE, 40, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P59945_54857_WIDE,
    { 59945, STM_PROGRESSIVE_SCAN, 1440, 540, 140, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1640, 558, 54857000, STM_SYNC_POSITIVE, 60, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P49999_54857_WIDE,
    { 49999, STM_PROGRESSIVE_SCAN, 1440, 540, 140, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1640, 669, 54857000, STM_SYNC_POSITIVE, 60, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P59945_54857,
    { 59945, STM_PROGRESSIVE_SCAN, 960, 540, 140, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1640, 558, 54857000, STM_SYNC_POSITIVE, 60, STM_SYNC_POSITIVE, 2}
  },
  { STM_TIMING_MODE_540P49999_54857,
    { 49999, STM_PROGRESSIVE_SCAN, 960, 540, 140, 11, STM_OUTPUT_STD_NTG5, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 1640, 669, 54857000, STM_SYNC_POSITIVE, 60, STM_SYNC_POSITIVE, 2}
  },

  /*
   * CEA861 pixel repeated modes for SD and ED modes. These can only be set on the
   * HDMI output, to match a particular analogue mode on the master output.
   *
   * Note: that pixel aspect ratios are not given for the 2880 modes as this is
   * variable dependent on the pixel repeat.
   */
  { STM_TIMING_MODE_480I59940_27000,
    { 59940, STM_INTERLACED_SCAN, 1440, 480, 238, 19, STM_OUTPUT_STD_CEA861, {PIX_AR_8_9, PIX_AR_32_27, PIX_AR_NULL}, {6,7,0} },
    { 1716, 525, 27000000, STM_SYNC_NEGATIVE, 124, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_576I50000_27000,
    { 50000, STM_INTERLACED_SCAN, 1440, 576, 264, 23, STM_OUTPUT_STD_CEA861, {PIX_AR_16_15, PIX_AR_64_45, PIX_AR_NULL}, {21,22,0} },
    { 1728, 625, 27000000, STM_SYNC_NEGATIVE, 126, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_480I59940_54000,
    { 59940, STM_INTERLACED_SCAN, 2880, 480, 476, 19, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_NULL, PIX_AR_NULL}, {10,11,0} },
    { 3432, 525, 54000000, STM_SYNC_NEGATIVE, 248, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_576I50000_54000,
    { 50000, STM_INTERLACED_SCAN, 2880, 576, 528, 23, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_NULL, PIX_AR_NULL}, {25,26,0} },
    { 3456, 625, 54000000, STM_SYNC_NEGATIVE, 252, STM_SYNC_NEGATIVE, 3}
  },
  { STM_TIMING_MODE_480P59940_54000,
    { 59940, STM_PROGRESSIVE_SCAN, 1440, 480, 244, 37, STM_OUTPUT_STD_CEA861, {PIX_AR_8_9, PIX_AR_32_27, PIX_AR_NULL}, {14,15,0} },
    { 1716, 525, 54000000, STM_SYNC_NEGATIVE, 124, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_480P60000_54054,
    { 60000, STM_PROGRESSIVE_SCAN, 1440, 480, 244, 37, STM_OUTPUT_STD_CEA861, {PIX_AR_8_9, PIX_AR_32_27, PIX_AR_NULL}, {14,15,0} },
    { 1716, 525, 54054000, STM_SYNC_NEGATIVE, 124, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_576P50000_54000,
    { 50000, STM_PROGRESSIVE_SCAN, 1440, 576, 264, 45, STM_OUTPUT_STD_CEA861, {PIX_AR_16_15, PIX_AR_64_45, PIX_AR_NULL}, {29,30,0} },
    { 1728, 625, 54000000, STM_SYNC_NEGATIVE, 128, STM_SYNC_NEGATIVE, 5}
  },
  { STM_TIMING_MODE_480P59940_108000,
    { 59940, STM_PROGRESSIVE_SCAN, 2880, 480, 488, 37, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_NULL, PIX_AR_NULL}, {35,36,0} },
    { 3432, 525, 108000000, STM_SYNC_NEGATIVE, 248, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_480P60000_108108,
    { 60000, STM_PROGRESSIVE_SCAN, 2880, 480, 488, 37, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_NULL, PIX_AR_NULL}, {35,36,0} },
    { 3432, 525, 108108000, STM_SYNC_NEGATIVE, 248, STM_SYNC_NEGATIVE, 6}
  },
  { STM_TIMING_MODE_576P50000_108000,
    { 50000, STM_PROGRESSIVE_SCAN, 2880, 576, 528, 45, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_NULL, PIX_AR_NULL}, {37,38,0} },
    { 3456, 625, 108000000, STM_SYNC_NEGATIVE, 256, STM_SYNC_NEGATIVE, 5}
  },

  /*
   * 297MHz pixel clock based CEA modes
   */
  { STM_TIMING_MODE_1080P119880_296703,
    { 119880, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,63,78} },
    { 2200, 1125, 296703297, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P120000_297000,
    { 120000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,63,78} },
    { 2200, 1125, 297000000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },
  { STM_TIMING_MODE_1080P100000_297000,
    { 100000, STM_PROGRESSIVE_SCAN, 1920, 1080, 192, 42, STM_OUTPUT_STD_CEA861, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,64,77} },
    { 2640, 1125, 297000000, STM_SYNC_POSITIVE, 44, STM_SYNC_POSITIVE, 5}
  },

  /*
   * HDMI extended 4Kx2K modes
   */
  { STM_TIMING_MODE_4K2K29970_296703,
    { 29970, STM_PROGRESSIVE_SCAN, 3840, 2160, 384, 83, STM_OUTPUT_STD_HDMI_LLC_EXT, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,95,105} },
    { 4400, 2250, 296703297, STM_SYNC_POSITIVE, 88, STM_SYNC_POSITIVE, 10}
  },
  { STM_TIMING_MODE_4K2K30000_297000,
    { 30000, STM_PROGRESSIVE_SCAN, 3840, 2160, 384, 83, STM_OUTPUT_STD_HDMI_LLC_EXT, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,95,105} },
    { 4400, 2250, 297000000, STM_SYNC_POSITIVE, 88, STM_SYNC_POSITIVE, 10}
  },
  { STM_TIMING_MODE_4K2K25000_297000,
    { 25000, STM_PROGRESSIVE_SCAN, 3840, 2160, 384, 83, STM_OUTPUT_STD_HDMI_LLC_EXT, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,94,104} },
    { 5280, 2250, 297000000, STM_SYNC_POSITIVE, 88, STM_SYNC_POSITIVE, 10}
  },
  { STM_TIMING_MODE_4K2K23980_296703,
    { 23980, STM_PROGRESSIVE_SCAN, 3840, 2160, 384, 83, STM_OUTPUT_STD_HDMI_LLC_EXT, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,93,103} },
    { 5500, 2250, 296703297, STM_SYNC_POSITIVE, 88, STM_SYNC_POSITIVE, 10}
  },
  { STM_TIMING_MODE_4K2K24000_297000,
    { 24000, STM_PROGRESSIVE_SCAN, 3840, 2160, 384, 83, STM_OUTPUT_STD_HDMI_LLC_EXT, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_4_3}, {0,93,103} },
    { 5500, 2250, 297000000, STM_SYNC_POSITIVE, 88, STM_SYNC_POSITIVE, 10}
  },
  { STM_TIMING_MODE_4K2K24000_297000_WIDE,
    { 24000, STM_PROGRESSIVE_SCAN, 4096, 2160, 384, 83, STM_OUTPUT_STD_HDMI_LLC_EXT, {PIX_AR_NULL, PIX_AR_SQUARE, PIX_AR_NULL}, {0,0,0} },
    { 5500, 2250, 297000000, STM_SYNC_POSITIVE, 88, STM_SYNC_POSITIVE, 10}
  },

};


COutput::COutput(const char     *name,
                 uint32_t        id,
                 CDisplayDevice* pDev,
                 uint32_t        timingID)
{
  m_name             = name;
  m_ID               = id;
  m_pDisplayDevice   = pDev;
  m_ulTimingID       = timingID;
  m_pSlavedOutputs   = 0;
  m_ulCapabilities   = 0;

  m_VideoSource    = STM_VIDEO_SOURCE_MAIN_COMPOSITOR;
  m_AudioSource    = STM_AUDIO_SOURCE_NONE;
  m_ulOutputFormat = 0;
  m_ulMaxPixClock  = 0;
  m_LastVTGEvent   = STM_TIMING_EVENT_NONE;

  m_LastVTGEventTime   = (stm_time64_t)0;
  m_fieldframeDuration = (stm_time64_t)0;
  m_displayStatus      = STM_DISPLAY_DISCONNECTED;
  m_bIsSuspended       = false;
  m_bIsStarted         = false;
  m_bDacPowerDown      = false;

  m_signalRange    = STM_SIGNAL_FILTER_SAV_EAV;

  m_DisplayAspectRatio.numerator   = 0;
  m_DisplayAspectRatio.denominator = 0;

  vibe_os_zero_memory(&m_CurrentOutputMode, sizeof(stm_display_mode_t));

  m_bForceColor        = false;
  m_uForcedOutputColor = 0x00101010; // Default value is video black

  m_lock = vibe_os_create_resource_lock();
}

COutput::~COutput(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  delete [] m_pSlavedOutputs;
  vibe_os_delete_resource_lock(m_lock);
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool COutput::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if(!(m_pSlavedOutputs = new COutput *[m_pDisplayDevice->GetNumberOfOutputs()]))
    return false;

  for (uint32_t i = 0; i < m_pDisplayDevice->GetNumberOfOutputs(); i++)
  {
    m_pSlavedOutputs[i] = 0L;
  }

  TRC( TRC_ID_MAIN_INFO, "%s capabilities = 0x%08x",m_name,m_ulCapabilities );

  if(m_ulCapabilities & OUTPUT_CAPS_PLANE_MIXER)
  {
    /*
     * Default display aspect ratio used for plane auto aspect ratio conversion
     * is 16:9
     */
    m_DisplayAspectRatio.numerator   = 16;
    m_DisplayAspectRatio.denominator = 9;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void COutput::CleanUp(void)
{
  /*
   * Overridden by subclasses, particularly where slaved outputs need to
   * do stuff before the master gets deleted and where the destructor may be
   * too late.
   */
}


bool COutput::GetDisplayStatus(stm_display_output_connection_status_t *status) const
{
  *status = m_displayStatus;

  if(m_bIsSuspended)
    return false;

  return true;
}


void COutput::SetDisplayStatus(stm_display_output_connection_status_t status)
{
  if(!m_bIsSuspended)
    m_displayStatus = status;
}


bool COutput::RegisterSlavedOutput(COutput *pOutput)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pOutput)
    return true;

  if(pOutput->GetID() >= m_pDisplayDevice->GetNumberOfOutputs())
    return false;

  m_pSlavedOutputs[pOutput->GetID()] = pOutput;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool COutput::UnRegisterSlavedOutput(COutput *pOutput)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pOutput)
    return true;

  if(pOutput->GetID() >= m_pDisplayDevice->GetNumberOfOutputs())
    return false;

  m_pSlavedOutputs[pOutput->GetID()] = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


const stm_display_mode_t* COutput::GetModeParamsLine(stm_display_mode_id_t mode) const
{
  for (int i=0; i < STM_TIMING_MODE_COUNT; i++)
  {
    if (ModeParamsTable[i].mode_id == mode)
    {
      if((m_ulMaxPixClock != 0) &&
         (ModeParamsTable[i].mode_timing.pixel_clock_freq > m_ulMaxPixClock))
      {
        return 0;
      }

      TRC( TRC_ID_UNCLASSIFIED, "COutput::GetModeParamsLine mode found." );
      return SupportedMode(&ModeParamsTable[i]);
    }
  }

  TRC( TRC_ID_UNCLASSIFIED, "COutput::GetModeParamsLine no mode found." );
  return 0;
}


const stm_display_mode_t* COutput::FindMode(uint32_t uXRes, uint32_t uYRes, uint32_t uMinLines, uint32_t uMinPixels, uint32_t uPixClock, stm_scan_type_t scanType) const
{
  /*
   * Allow for some maths rounding errors in the pixel clock comparison :
   * Default clock margin for mode having low and high pixel clock rate is 6 KHz.
   * Margin for modes having 'ultra' high pixel clock rate (up to 148MHz modes)
   * is 36 KHz.
   */
  uint32_t clockMargin = (uPixClock > 148500000) ? 36000 : 6000;
  int i;

  TRC( TRC_ID_UNCLASSIFIED, "COutput::FindMode x=%u y=%u pixclk=%u interlaced=%s.", uXRes,uYRes,uPixClock,(scanType==STM_INTERLACED_SCAN)?"true":"false" );

  for (i=0; i < STM_TIMING_MODE_COUNT; i++)
  {
    if(ModeParamsTable[i].mode_params.active_area_width != uXRes)
      continue;

    if(ModeParamsTable[i].mode_params.active_area_height != uYRes)
      continue;

    if(uPixClock < ModeParamsTable[i].mode_timing.pixel_clock_freq-clockMargin ||
       uPixClock > ModeParamsTable[i].mode_timing.pixel_clock_freq+clockMargin )
      continue;

    if((m_ulMaxPixClock != 0) && (uPixClock > m_ulMaxPixClock+clockMargin))
      continue;

    if(ModeParamsTable[i].mode_params.scan_type != scanType)
      continue;

    /*
     * Unfortunately 1080i@59.94Hz and 1080i@50Hz will both pass all the above
     * tests; so we need to check the total lines. To make this a bit easier
     * the input parameter is the minimum number of lines required to match the
     * mode. This allows someone to pass is 0, to match the first mode that
     * matches all the other more standard parameters.
     */
    if(ModeParamsTable[i].mode_timing.lines_per_frame < uMinLines)
      continue;

    /*
     * Same idea as above but for the total number of pixels per line
     * this lets us distinguish between 720p@50Hz and 720p@60Hz
     */
    if(ModeParamsTable[i].mode_timing.pixels_per_line < uMinPixels)
      continue;

    TRC( TRC_ID_UNCLASSIFIED, "COutput::FindMode mode found." );
    return SupportedMode(&ModeParamsTable[i]);
  }

  TRC( TRC_ID_UNCLASSIFIED, "COutput::FindMode no mode found." );
  return 0;
}



const stm_display_mode_t* COutput::GetSlavedOutputsSupportedMode(const stm_display_mode_t *mode) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  const stm_display_mode_t* tmpMode = 0;

  for(uint8_t i=0; (!tmpMode && (i < m_pDisplayDevice->GetNumberOfOutputs())); i++)
  {
    if(m_pSlavedOutputs[i])
      tmpMode = m_pSlavedOutputs[i]->SupportedMode(mode);
  }
  if (!tmpMode)
    TRC( TRC_ID_MAIN_INFO, "mode %ssupported by slaved output[s]", tmpMode?"":"not ");

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return tmpMode;
}

OutputResults COutput::Start(const stm_display_mode_t* pMode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_CurrentOutputMode = *pMode;
  m_bIsStarted = true;

  TRC( TRC_ID_MAIN_INFO, "mode %d @ %p", m_CurrentOutputMode.mode_id, this );

  m_fieldframeDuration = GetFieldOrFrameDurationFromMode(&m_CurrentOutputMode);

  TRC( TRC_ID_MAIN_INFO, "COutput::Start m_fieldframeDuration = %lld (us/ticks)", m_fieldframeDuration );

  for(uint32_t i=0; i < m_pDisplayDevice->GetNumberOfOutputs(); i++)
  {
    if(m_pSlavedOutputs[i])
      m_pSlavedOutputs[i]->UpdateOutputMode(m_CurrentOutputMode);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return STM_OUT_OK;
}


bool COutput::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  m_fieldframeDuration = 0;
  m_LastVTGEvent       = STM_TIMING_EVENT_NONE;
  m_LastVTGEventTime   = (stm_time64_t)0;
  m_bIsSuspended       = false;
  m_bIsStarted         = false;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool COutput::SetSlavedOutputsVTGSyncs(const stm_display_mode_t *mode)
{
  bool RetOk = true;

  TRCIN( TRC_ID_MAIN_INFO, "" );
  for(uint32_t i=0; i < m_pDisplayDevice->GetNumberOfOutputs(); i++)
  {
    if(RetOk && m_pSlavedOutputs[i])
    {
      RetOk = m_pSlavedOutputs[i]->SetVTGSyncs(mode);
    }
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return RetOk;
}


void COutput::StopSlavedOutputs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  for(uint32_t i=0; i < m_pDisplayDevice->GetNumberOfOutputs(); i++)
  {
    if(m_pSlavedOutputs[i] && m_pSlavedOutputs[i]->IsStarted())
      m_pSlavedOutputs[i]->Stop();
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void COutput::Suspend(void)
{
  m_bIsSuspended     = true;
  m_LastVTGEvent     = STM_TIMING_EVENT_NONE;
  m_LastVTGEventTime = (stm_time64_t)0;
}


void COutput::Resume(void)
{
  m_bIsSuspended = false;
}


uint32_t COutput::SetCompoundControl(stm_output_control_t ctrl, void *newVal)
{
  uint32_t result = STM_OUT_NO_CTRL;

  switch(ctrl)
  {
    case OUTPUT_CTRL_DISPLAY_ASPECT_RATIO:
      /*
       * Only support the aspect ratio control on an output that mixes planes
       * (as defined in the STKPI pre-condition for this control). We implement
       * it here in the generic code instead of CSTmMasterOutput as the TV
       * panel output classes are expected to inherit directly from here.
       */
      if(m_ulCapabilities & OUTPUT_CAPS_PLANE_MIXER)
      {
        stm_rational_t *ar = (stm_rational_t *)newVal;
        if(ar->numerator == 0 || ar->denominator == 0)
        {
          TRC( TRC_ID_ERROR, "Invalid Aspect ratio value, one or both values are zero" );
          result = STM_OUT_INVALID_VALUE;
        }
        else
        {
          TRC( TRC_ID_MAIN_INFO, "Display Aspect Ratio %d:%d",ar->numerator,ar->denominator );
          vibe_os_lock_resource(m_lock);
            m_DisplayAspectRatio = *ar;
          vibe_os_unlock_resource(m_lock);
          result = STM_OUT_OK;
        }
      }
      break;
    default:
      break;
  }

  return result;
}


uint32_t COutput::GetCompoundControl(stm_output_control_t ctrl, void *val) const
{
  uint32_t result = STM_OUT_NO_CTRL;

  switch(ctrl)
  {
    case OUTPUT_CTRL_DISPLAY_ASPECT_RATIO:
      if(m_ulCapabilities & OUTPUT_CAPS_PLANE_MIXER)
      {
        /*
         * The rational structure is obviously not accessed with a single bus
         * read, so if this is being accessed in interrupt context by the plane
         * updates we need to protect it with a spinlock/interrupt disable.
         */
        vibe_os_lock_resource(m_lock);
          *(stm_rational_t*)val = m_DisplayAspectRatio;
        vibe_os_unlock_resource(m_lock);
        result = STM_OUT_OK;
      }
      break;
    default:
      break;
  }

  return result;
}


uint32_t COutput::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  uint32_t result = STM_OUT_NO_CTRL;

  switch(ctrl)
  {
    case OUTPUT_CTRL_FORCED_RGB_VALUE:
      if(m_ulCapabilities & OUTPUT_CAPS_FORCED_COLOR)
      {
        *val = m_uForcedOutputColor;
        result = STM_OUT_OK;
      }
      break;
    case OUTPUT_CTRL_FORCE_COLOR:
      if(m_ulCapabilities & OUTPUT_CAPS_FORCED_COLOR)
      {
        *val = m_bForceColor?1:0;
        result = STM_OUT_OK;
      }
      break;
    default:
      TRC( TRC_ID_MAIN_INFO, "Attempt to access unexpected control %d", ctrl );
      *val = 0;
      break;
  }

  return result;
}


/*
 * Default null implementations of virtual methods that may not be required on
 * some output types.
 */
bool COutput::CanShowPlane(const CDisplayPlane *) { return false; }

bool COutput::ShowPlane   (const CDisplayPlane *) { return false; }

void COutput::HidePlane   (const CDisplayPlane *) {}

bool COutput::SetPlaneDepth(const CDisplayPlane *, int depth, bool activate) { return false; }

bool COutput::GetPlaneDepth(const CDisplayPlane *, int *depth) const { return false; }

stm_display_metadata_result_t COutput::QueueMetadata(stm_display_metadata_t *) { return STM_METADATA_RES_UNSUPPORTED_TYPE; }

void COutput::FlushMetadata(stm_display_metadata_type_t) {}

void COutput::UpdateHW() {}

TuningResults COutput::SetTuning(uint16_t service, void *inputList, uint32_t inputListSize, void *outputList, uint32_t outputListSize)
{
    // Default, return invalid for an unsupported
    return TUNING_SERVICE_NOT_SUPPORTED;
}


void COutput::SetClockReference(stm_clock_ref_frequency_t refClock, int error_ppm) {}

void COutput::SoftReset() {}

stm_time64_t COutput::GetFieldOrFrameDurationFromMode(const stm_display_mode_t *mode)
{
  /*
   * Note: This returns the number of system clock ticks per frame as
   *       defined by the frame rate in the mode parameters, which is
   *       specified in mHz hence the "*1000" in the first parameter.
   */
  return vibe_os_div64(((uint64_t)vibe_os_get_one_second()*1000LL),
                       (uint64_t)mode->mode_params.vertical_refresh_rate);
}

////////////////////////////////////////////////////////////////////////////
// C Display output interface
//

extern "C" {

int stm_display_output_get_name(stm_display_output_h output, const char **name)
{
  COutput *pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(name))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  *name = pOut->GetName();

  STKPI_UNLOCK(output->parent_dev);

  return 0;
}


int stm_display_output_get_device_id(stm_display_output_h output, uint32_t *id)
{
  COutput *pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(id))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  *id = pOut->GetParentDevice()->GetID();

  STKPI_UNLOCK(output->parent_dev);

  return 0;
}


int stm_display_output_get_capabilities(stm_display_output_h output, uint32_t *caps)
{
  COutput *pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(caps))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  *caps = pOut->GetCapabilities();

  STKPI_UNLOCK(output->parent_dev);

  return 0;
}


int stm_display_output_get_display_mode(stm_display_output_h  output,
                    stm_display_mode_id_t mode_id,
                    stm_display_mode_t   *mode)
{
  COutput *pOut = NULL;
  const stm_display_mode_t *mode_line;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(mode))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s, mode_id : %d", pOut->GetName(), mode_id);
  mode_line = pOut->GetModeParamsLine(mode_id);

  if ( mode_line )
    {
      TRC(TRC_ID_API_OUTPUT, "mode_id : %d, mode_params.scan_type : %d", mode_line->mode_id, mode_line->mode_params.scan_type );
    }

  STKPI_UNLOCK(output->parent_dev);

  if(!mode_line)
    return -ENOTSUP;

  *mode = *mode_line;

  return 0;
}


int stm_display_output_find_display_mode(stm_display_output_h output,
                     uint32_t             xres,
                     uint32_t             yres,
                     uint32_t             minlines,
                     uint32_t             minpixels,
                     uint32_t             pixclock,
                     stm_scan_type_t      scantype,
                     stm_display_mode_t  *mode)
{
  COutput *pOut = NULL;
  const stm_display_mode_t *mode_line;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(mode))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  mode_line = pOut->FindMode(xres, yres, minlines, minpixels, pixclock, scantype);

  STKPI_UNLOCK(output->parent_dev);

  if(!mode_line)
    return -ENOTSUP;

  *mode = *mode_line;
  return 0;
}


int stm_display_output_start(stm_display_output_h output, const stm_display_mode_t *pModeLine)
{
  COutput *pOut = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  pOut = output->handle;

  if(!CHECK_ADDRESS(pModeLine))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  switch(pOut->Start(pModeLine))
  {
    case STM_OUT_OK:
      break;
    case STM_OUT_BUSY:
      ret = -EBUSY;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(output->parent_dev);

  return ret;
}


int stm_display_output_stop(stm_display_output_h output)
{
  COutput *pOut = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  pOut = output->handle;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  ret = pOut->Stop()?0:-EBUSY;

  STKPI_UNLOCK(output->parent_dev);

  return ret;
}


int stm_display_output_set_control(stm_display_output_h output, stm_output_control_t ctrl, uint32_t newVal)
{
  COutput *pOut = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  pOut = output->handle;
  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  switch(pOut->SetControl(ctrl, newVal))
  {
    case STM_OUT_NO_CTRL:
      ret = -ENOTSUP;
      break;
    case STM_OUT_INVALID_VALUE:
      ret = -ERANGE;
      break;
    default:
      break;
  }

  STKPI_UNLOCK(output->parent_dev);

  return ret;
}


int stm_display_output_get_control(stm_display_output_h output, stm_output_control_t ctrl, uint32_t *ctrlVal)
{
  COutput *pOut = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(ctrlVal))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  if(pOut->GetControl(ctrl, ctrlVal) == STM_OUT_NO_CTRL)
    ret = -ENOTSUP;

  STKPI_UNLOCK(output->parent_dev);

  return ret;
}


int stm_display_output_set_compound_control(stm_display_output_h output, stm_output_control_t ctrl, void *newVal)
{
  COutput *pOut = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  pOut = output->handle;

  if(!CHECK_ADDRESS(newVal))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  switch(pOut->SetCompoundControl(ctrl, newVal))
  {
    case STM_OUT_NO_CTRL:
      ret = -ENOTSUP;
      break;
    case STM_OUT_INVALID_VALUE:
      ret = -ERANGE;
      break;
    default:
      break;
  }

  STKPI_UNLOCK(output->parent_dev);

  return ret;
}


int stm_display_output_get_compound_control(stm_display_output_h output, stm_output_control_t ctrl, void *ctrlVal)
{
  COutput *pOut = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(ctrlVal))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  if(pOut->GetCompoundControl(ctrl, ctrlVal) == STM_OUT_NO_CTRL)
    ret = -ENOTSUP;

  STKPI_UNLOCK(output->parent_dev);

  return ret;
}


int stm_display_output_queue_metadata(stm_display_output_h           output,
                          stm_display_metadata_t        *data)
{
  COutput *pOut = NULL;
  stm_display_metadata_result_t res;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  pOut = output->handle;

  if(!CHECK_ADDRESS(data))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  res = pOut->QueueMetadata(data);

  STKPI_UNLOCK(output->parent_dev);

  switch(res)
  {
    case STM_METADATA_RES_UNSUPPORTED_TYPE:
      return -ENOTSUP;
    case STM_METADATA_RES_TIMESTAMP_IN_PAST:
      return -ETIME;
    case STM_METADATA_RES_INVALID_DATA:
      return -EBADMSG;
    case STM_METADATA_RES_QUEUE_BUSY:
      return -EBUSY;
    case STM_METADATA_RES_QUEUE_UNAVAILABLE:
      return -EAGAIN;
    default:
      break;
  }
  return 0;
}


int stm_display_output_flush_metadata(stm_display_output_h output, stm_display_metadata_type_t type)
{
  COutput *pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  pOut = output->handle;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  pOut->FlushMetadata(type);

  STKPI_UNLOCK(output->parent_dev);

  return 0;
}


int stm_display_output_get_current_display_mode(stm_display_output_h output, stm_display_mode_t *mode)
{
  COutput *pOut = NULL;
  const stm_display_mode_t *current_mode;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  current_mode = pOut->GetCurrentDisplayMode();

  if ( current_mode ) {
    TRC(TRC_ID_API_OUTPUT, "mode_id : %d, mode_params.scan_type : %d", current_mode->mode_id, current_mode->mode_params.scan_type );
  }

  STKPI_UNLOCK(output->parent_dev);

  if(!current_mode)
    return -ENOMSG;

  *mode = *current_mode;

  return 0;
}


int stm_display_output_set_clock_reference(stm_display_output_h output, stm_clock_ref_frequency_t refClock, int refClockError)
{
  COutput *pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(output->parent_dev))
    return -EAGAIN;

  pOut = output->handle;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  pOut->SetClockReference(refClock,refClockError);

  STKPI_UNLOCK(output->parent_dev);

  return 0;
}


int stm_display_output_get_timing_identifier(stm_display_output_h output, uint32_t *tid)
{
  COutput *pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(tid))
    return -EFAULT;

  if(STKPI_LOCK(output->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  /*
   * Only query the internal object's timing ID if it is a timing master,
   * slaved outputs may store their master's timing identifier so their device
   * UpdateHW method is called during a display update.
   */
  if(pOut->GetCapabilities() & OUTPUT_CAPS_DISPLAY_TIMING_MASTER)
    *tid = pOut->GetTimingID();
  else
    *tid = 0;

  STKPI_UNLOCK(output->parent_dev);

  return (*tid == 0)?-ENOTSUP:0;
}


void stm_display_output_handle_interrupts(stm_display_output_h output)
{
  COutput *pOut = NULL;

  ASSERTF(stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE),(""));

  pOut = output->handle;
  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  pOut->HandleInterrupts();
}


void stm_display_output_soft_reset(stm_display_output_h output)
{
  COutput *pOut = NULL;

  ASSERTF(stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE),(""));

  pOut = output->handle;
  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());

  pOut->SoftReset();
}


void stm_display_output_get_last_timing_event(stm_display_output_h output, uint32_t *event, stm_time64_t *timestamp)
{
  COutput *pOut = NULL;

  ASSERTF(stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE),(""));
  pOut = output->handle;
  ASSERTF(CHECK_ADDRESS(event),(""));
  ASSERTF(CHECK_ADDRESS(timestamp),(""));

  /*
   * This may be called from interrupt so don't take the lock.
   * Note there is a clear race condition here, you might get
   * a vsync between reading the field and the time. However
   * this is not very damaging, so we are leaving it for the moment.
   */
  *event     = pOut->GetCurrentVTGEvent();
  *timestamp = pOut->GetCurrentVTGEventTime();
  TRC(TRC_ID_API_OUTPUT, "output : %s, event : 0x%04x, timestamp : %lld", pOut->GetName(), *event, *timestamp);
}


int stm_display_output_get_connection_status(stm_display_output_h output, stm_display_output_connection_status_t *status)
{
  COutput *pOut = NULL;

  ASSERTF(CHECK_ADDRESS(status),(""));

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  /*
   * Note that this is called from the primary interrupt handler so we
   * must not take the mutex and we can't use call
   * the stm_display_device_is_suspended() because it's locking the
   * device also.
   */
  if(!pOut->GetDisplayStatus(status))
    return -EAGAIN;

  TRC(TRC_ID_API_OUTPUT, "output : %s, status : %d", pOut->GetName(), *status);

  return 0;
}


void stm_display_output_set_connection_status(stm_display_output_h output, stm_display_output_connection_status_t status)
{
  COutput *pOut = NULL;

  ASSERTF(stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE),(""));
  pOut = output->handle;
  TRC(TRC_ID_API_OUTPUT, "output : %s", pOut->GetName());
  /*
   * Note that this is called from the primary interrupt handler so we
   * must not take the mutex.
   */
  pOut->SetDisplayStatus(status);
}


void stm_display_output_close(stm_display_output_h output)
{
  if(!output)
    return;

  ASSERTF(stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE),(""));

  TRC(TRC_ID_API_OUTPUT, "output : %s", output->handle->GetName());
  stm_coredisplay_magic_clear(output);

  /*
   * Remove object instance from the registry before exiting.
   */
  if(stm_registry_remove_object(output) < 0)
    TRC( TRC_ID_ERROR, "failed to remove output instance from the registry" );

  /*
   * Release the structure's memory.
   */
  delete output;
}

} // extern "C"
