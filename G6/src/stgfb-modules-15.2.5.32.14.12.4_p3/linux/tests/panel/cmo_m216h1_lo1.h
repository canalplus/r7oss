/***********************************************************************
 *
 * File: linux/tests/panel/cmo_m216h1_lo1.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

/* Define to prevent recursive inclusion */
#ifndef __CMO_M216H_LO1_H
#define __CMO_M216H_LO1_H

#include "panel.h"

const PanelParams_t PanelParams_CMO_M216H1LO1 =
// 0:CMO_M216H1_LO1 (DELL 1080P@60Hz)
{
  CMO_M216H1_LO1,
  "CMO_M216H1_LO1",
  0, //lvds_config
  0, //lookup_table1_p;
  0, //lookup_table2_p;
  0, //linear_color_remap_table_p;
  60, //pwr_to_de_delay_during_power_on;
  125, //de_to_bklt_on_delay_during_power_on;
  140, //bklt_to_de_off_delay_during_power_off;
  140, //de_to_pwr_delay_during_power_off;
  0, // enable_lut1;
  0, // enable_lut2;
  0, // afr_enable;
  0, // is_half_display_clock;
  STMFB_PANEL_DITHER_METHOD_NONE, // dither;
  STMFB_PANEL_FREERUN, //stmfbio_display_panel_lock_method lock_method;
  60000, //frame_rate;
  1920, //active_area_width;
  1080, //active_area_height;
  2200, //pixels_per_line;
  1125, //lines_per_frame;
  148500000, //pixel_clock_freq;
  112, //active_area_start_pixel;
  20, //active_area_start_line;
  8, //hsync_width;
  6, //vsync_width;
  0, // hsync_polarity;
  0, // vsync_polarity;
};

#endif /* __CMO_M216H_LO1_H */

