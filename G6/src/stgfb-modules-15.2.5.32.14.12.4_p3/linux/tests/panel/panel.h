/***********************************************************************
 *
 * File: linux/tests/panel/panel.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

#ifndef _PANEL_H
#define _PANEL_H

#include <linux/kernel/drivers/video/stmfb.h>

#define PACKED  __attribute__((packed))

typedef enum
{
    CMO_M216H1_LO1 = 0,
    CMO_M236H5_LOA,
    MAX_NUM_SUPPORTED_PANEL,
} SupportedPanelID;

#define DEFAULT_PANEL   CMO_M216H1_LO1

typedef struct
{
  unsigned int panelID;
  char panelname[30];
  stmfbio_display_panel_lvds_config_t* lvds_config;
  unsigned char* lookup_table1_p;
  unsigned char* lookup_table2_p;
  unsigned char* linear_color_remap_table_p;
  unsigned short pwr_to_de_delay_during_power_on;
  unsigned short de_to_bklt_on_delay_during_power_on;
  unsigned short bklt_to_de_off_delay_during_power_off;
  unsigned short de_to_pwr_delay_during_power_off;
  unsigned char enable_lut1;
  unsigned char enable_lut2;
  unsigned char afr_enable;
  unsigned char is_half_display_clock;
  stmfbio_display_panel_dither_mode dither;
  stmfbio_display_panel_lock_method lock_method;
  unsigned int vertical_refresh_rate;
  unsigned int active_area_width;
  unsigned int active_area_height;
  unsigned int pixels_per_line;
  unsigned int lines_per_frame;
  unsigned int pixel_clock_freq;
  unsigned int active_area_start_pixel;
  unsigned int active_area_start_line;
  unsigned int hsync_width;
  unsigned int vsync_width;
  unsigned char hsync_polarity;
  unsigned char vsync_polarity;
}PanelParams_t;

#endif /* _PANEL_H */
