/***********************************************************************
 *
 * File: include/stm_lvds_output.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_LVDS_OUTPUT_H
#define _STM_LVDS_OUTPUT_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_lvds_output.h
 *  \brief type definitions for LVDS output to TV panels including TV
 *         backlight support.
 */


/*! \enum    stm_display_panel_dither_mode_e
 *  \brief   Panel color dither methods
 */
typedef enum stm_display_panel_dither_mode_e
{
  STM_PANEL_DITHER_METHOD_NONE,    /*!< No Dither              */
  STM_PANEL_DITHER_METHOD_TRUNC,   /*!< Truncate the LSB       */
  STM_PANEL_DITHER_METHOD_ROUND,   /*!< Round off with the LSB */
  STM_PANEL_DITHER_METHOD_RANDOM,  /*!< Random dither          */
  STM_PANEL_DITHER_METHOD_SPATIAL, /*!< Spatial dither         */
} stm_display_panel_dither_mode_t;


/*! \enum    stm_display_panel_lock_method_e
 *  \brief   Options to lock the output display to an input source's timing
 */
typedef enum stm_display_panel_lock_method_e
{
  STM_PANEL_FREERUN,            /*!< Free run the display timing              */
  STM_PANEL_FIXED_FRAME_LOCK,   /*!< Use a fixed frame locking scheme         */
  STM_PANEL_DYNAMIC_FRAME_LOCK, /*!< Use a Dynamic frame locking (DFL) scheme */
  STM_PANEL_LOCK_LOAD,          /*!< Use a lock load scheme                   */
} stm_display_panel_lock_method_t;


/*! \struct  stm_display_panel_color_config_s
 *  \brief   Panel color configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_color_config_s
{
  uint8_t *lookup_table1_p;            /*!< */
  uint8_t *lookup_table2_p;            /*!< */
  bool     enable_lut1;                /*!< */
  bool     enable_lut2;                /*!< */
  uint8_t *linear_color_remap_table_p; /*!< */
}stm_display_panel_color_config_t;


/*! \struct  stm_display_panel_timing_config_s
 *  \brief   Panel timing configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_timing_config_s
{
  bool                            afr_enable;             /*!< */
  stm_display_panel_lock_method_t lock_method;            /*!< */
  bool                            is_half_display_clock;  /*!< */
}stm_display_panel_timing_config_t;


/*! \struct  stm_display_panel_spread_spectrum_config_s
 *  \brief   Panel spread spectrum clock configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_spread_spectrum_config_s
{
  bool      spread_spectrum_en;
  uint16_t  modulation_freq;
  uint16_t  modulation_depth;
}stm_display_panel_spread_spectrum_config_t;


/*! \struct  stm_display_panel_power_timing_config_s
 *  \brief   Panel power up/down timing configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_power_timing_config_s
{
  uint16_t pwr_to_de_delay_during_power_on;       /*!< */
  uint16_t de_to_bklt_on_delay_during_power_on;   /*!< */
  uint16_t bklt_to_de_off_delay_during_power_off; /*!< */
  uint16_t de_to_pwr_delay_during_power_off;      /*!< */
}stm_display_panel_power_timing_config_t;


/*! \enum    stm_display_panel_lvds_bus_width_select_e
 *  \brief   LVDS bus configuration
 */
typedef enum stm_display_panel_lvds_bus_width_select_e
{
  STM_PANEL_LVDS_SINGLE, /*!< */
  STM_PANEL_LVDS_DUAL,   /*!< */
  STM_PANEL_LVDS_QUAD,   /*!< */
} stm_display_panel_lvds_bus_width_select_t;


/*! \enum    stm_display_panel_lvds_bus_swap_e
 *  \brief   LVDS bus swap
 */
typedef enum stm_display_panel_lvds_bus_swap_e
{
  STM_PANEL_LVDS_SWAP_AB    = 1, /*!< */
  STM_PANEL_LVDS_SWAP_CD    = 2, /*!< */
  STM_PANEL_LVDS_SWAP_AB_CD = 3  /*!< */
} stm_display_panel_lvds_bus_swap_t;


/*! \enum    stm_display_panel_lvds_pair_swap_e
 *  \brief   LVDS swap +- signal pairs
 */
typedef enum stm_display_panel_lvds_pair_swap_e
{
  STM_PANEL_LVDS_SWAP_PAIR0_PN = (1L<<0), /*!< */
  STM_PANEL_LVDS_SWAP_PAIR1_PN = (1L<<1), /*!< */
  STM_PANEL_LVDS_SWAP_PAIR2_PN = (1L<<2), /*!< */
  STM_PANEL_LVDS_SWAP_PAIR3_PN = (1L<<3), /*!< */
  STM_PANEL_LVDS_SWAP_PAIR4_PN = (1L<<4), /*!< */
  STM_PANEL_LVDS_SWAP_PAIR5_PN = (1L<<5), /*!< */
  STM_PANEL_LVDS_SWAP_CLOCK_PN = (1L<<6), /*!< */
} stm_display_panel_lvds_pair_swap_t;


/*! \struct  stm_display_panel_lvds_ch_swap_config_s
 *  \brief   LVDS bus channel swap configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_lvds_ch_swap_config_s
{
  stm_display_panel_lvds_bus_swap_t  swap_options;   /*!< */
  stm_display_panel_lvds_pair_swap_t pair_swap_ch_a; /*!< */
  stm_display_panel_lvds_pair_swap_t pair_swap_ch_b; /*!< */
  stm_display_panel_lvds_pair_swap_t pair_swap_ch_c; /*!< */
  stm_display_panel_lvds_pair_swap_t pair_swap_ch_d; /*!< */
}stm_display_panel_lvds_ch_swap_config_t;


/*! \struct  stm_display_panel_lvds_signal_pol_config_s
 *  \brief   LVDS ancillary signal polarity
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_lvds_signal_pol_config_s
{
  stm_sync_polarity_t hs_pol;  /*!< hsync polarity              */
  stm_sync_polarity_t vs_pol;  /*!< vsync polarity              */
  stm_sync_polarity_t de_pol;  /*!< data enable polarity        */
  stm_sync_polarity_t odd_pol; /*!< odd not even field polarity */
} stm_display_panel_lvds_signal_pol_config_t;


/*! \enum    stm_display_panel_lvds_map_e
 *  \brief   LVDS pin mapping
 */
typedef enum stm_display_panel_lvds_map_e
{
  STM_PANEL_LVDS_MAP_STANDARD1, /*!< */
  STM_PANEL_LVDS_MAP_STANDARD2, /*!< */
  STM_PANEL_LVDS_MAP_CUSTOM     /*!< */
} stm_display_panel_lvds_map_t;


/*! \enum    stm_display_lvds_common_mode_voltage_e
 *  \brief   LVDS bias control common mode voltage
 */
typedef enum stm_display_lvds_common_mode_voltage_e
{
  STM_PANEL_LVDS_1_25V, /*!< 1.25v */
  STM_PANEL_LVDS_1_10V, /*!< 1.10v */
  STM_PANEL_LVDS_1_35V  /*!< 1.35v */
}stm_display_lvds_common_mode_voltage_t;


/*! \enum    stm_display_lvds_bias_current_e
 *  \brief   LVDS bias control current
 */
typedef enum stm_display_lvds_bias_current_e
{
  STM_PANEL_LVDS_0_5_MA, /*!< 0.5 mA */
  STM_PANEL_LVDS_1_0_MA, /*!< 1.0 mA */
  STM_PANEL_LVDS_1_5_MA, /*!< 1.5 mA */
  STM_PANEL_LVDS_2_0_MA, /*!< 2.0 mA */
  STM_PANEL_LVDS_2_5_MA, /*!< 2.5 mA */
  STM_PANEL_LVDS_3_0_MA, /*!< 3.0 mA */
  STM_PANEL_LVDS_3_5_MA, /*!< 3.5 mA */
  STM_PANEL_LVDS_4_0_MA  /*!< 4.0 mA */
}stm_display_lvds_bias_current_t;


/*! \struct  stm_display_panel_lvds_bias_control_s
 *  \brief   LVDS interface electrical configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_lvds_bias_control_s
{
  stm_display_lvds_common_mode_voltage_t  common_mode_voltage; /*!< */
  stm_display_lvds_bias_current_t         bias_current;        /*!< */
} stm_display_panel_lvds_bias_control_t;


/*! \struct  stm_display_panel_lvds_config_s
 *  \brief   LVDS panel configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_lvds_config_s
{
  stm_display_panel_lvds_bus_width_select_t   lvds_bus_width_sel;   /*!< */
  stm_display_panel_lvds_ch_swap_config_t     swap_options;         /*!< */
  stm_display_panel_lvds_signal_pol_config_t  signal_polarity;      /*!< */
  stm_display_panel_lvds_map_t                lvds_signal_map;      /*!< */
  uint32_t                                    custom_lvds_map[8];   /*!< */
  uint16_t                                    clock_data_adj;       /*!< */
  stm_display_panel_lvds_bias_control_t       bias_control_value;   /*!< */
} stm_display_panel_lvds_config_t;


/*! \enum    stm_display_panel_lvds_sec_map_to_main_e
 *  \brief   LVDS secondary stream to main data mapping
 */
typedef enum stm_display_panel_lvds_sec_map_to_main_e
{
  STM_PANEL_LVDS_MODE1, /*!< */
  STM_PANEL_LVDS_MODE2, /*!< */
  STM_PANEL_LVDS_MODE4  /*!< */
} stm_display_panel_lvds_sec_map_to_main_t;


/*! \struct  stm_display_panel_sec_stream_config_s
 *  \brief   Secondary stream configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_sec_stream_config_s
{
  bool                                     alpha_ch_round_up;   /*!< */
  stm_display_panel_dither_mode_t          dither_type;         /*!< */
  stm_display_panel_lvds_sec_map_to_main_t lvds_map_mode;       /*!< */
} stm_display_panel_sec_stream_config_t;


/*! \struct  stm_display_panel_config_s
 *  \brief   Panel output configuration
 *  \ctrlarg OUTPUT_CTRL_PANEL_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_config_params_s
{
  stm_display_panel_color_config_t           color_config;           /*!< */
  stm_display_panel_dither_mode_t            dither_type;            /*!< */
  stm_display_panel_timing_config_t          misc_timing_config;     /*!< */
  stm_display_panel_spread_spectrum_config_t spread_spectrum_config; /*!< */
  stm_display_panel_power_timing_config_t    panel_power_up_timing;  /*!< */

  stm_display_panel_lvds_config_t           *lvds_config_p;          /*!< */
  stm_display_panel_sec_stream_config_t     *sec_stream_config_p;    /*!< */
} stm_display_panel_config_t;


/*! \enum    stm_display_ledblu_ctrl_update_e
 *  \brief   LED backlight configuration update flags
 */
typedef enum stm_display_ledblu_ctrl_update_e
{
  STM_PANEL_UPDATE_NONE               = 0,       /*!< */
  STM_PANEL_UPDATE_CONFIG_PARAMS      = (1L<<0), /*!< */
  STM_PANEL_UPDATE_ZONE_CONTOUR_TABLE = (1L<<1), /*!< */
  STM_PANEL_UPDATE_UNIFORMITY_TABLE   = (1L<<2), /*!< */
  STM_PANEL_UPDATE_GAMMA_TABLE        = (1L<<3), /*!< */
  STM_PANEL_UPDATE_FLOOR_VALUE        = (1L<<4), /*!< */
  STM_PANEL_UPDATE_CEIL_VALUE         = (1L<<5), /*!< */
} stm_display_ledblu_ctrl_update_t;


/*! \struct  stm_display_ledblu_config_s
 *  \brief   LED backlight configuration
 *  \ctrlarg OUTPUT_CTRL_LEDBLU_CONFIGURE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_ledblu_config_s
{
  stm_display_ledblu_ctrl_update_t update_flags;       /*!< */
  uint8_t                         *config_params;      /*!< */
  uint8_t                         *zone_contour_table; /*!< */
  uint8_t                         *uniformity_table;   /*!< */
  uint8_t                         *gamma_table;        /*!< */
  uint8_t                          floor_value;        /*!< */
  uint8_t                          ceil_value;         /*!< */
} stm_display_ledblu_config_t;


/*! \enum    stm_display_panel_lookup_table_ctrl_e
 *  \brief   LUT control values
 *  \ctrlarg OUTPUT_CTRL_PANEL_LOOKUP_TABLE_ENABLE
 *  \apis    ::stm_display_output_set_control(),
 *           ::stm_display_output_get_control()
 */
typedef enum stm_display_panel_lookup_table_ctrl_e
{
  STM_PANEL_LUT1_DISABLE,
  STM_PANEL_LUT1_ENABLE,
  STM_PANEL_LUT2_DISABLE,
  STM_PANEL_LUT2_ENABLE,
} stm_display_panel_lookup_table_ctrl_t;


/*! \struct  stm_display_panel_lookup_table_s
 *  \brief   Lookup table definition
 *  \ctrlarg OUTPUT_CTRL_PANEL_WRITE_LOOKUP_TABLE,
 *           OUTPUT_CTRL_PANEL_READ_LOOKUP_TABLE
 *  \apis    ::stm_display_output_set_compound_control(),
 *           ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_lookup_table_s
{
  uint8_t  lookup_table_sel; /*!< Table number required, form 1..n */
  uint8_t* lookup_table_p;   /*!< Table data                       */
} stm_display_panel_lookup_table_t;


/*! \enum    stm_display_panel_ledblu_range_sel_e
 *  \brief   LED backlight control value identifiers
 */
typedef enum stm_display_panel_ledblu_range_sel_e
{
  STM_PANEL_LEDBLU_FLOOR_BRIGHTNESS, /*!< */
  STM_PANEL_LEDBLU_BRIGHTNESS,       /*!< */
  STM_PANEL_LEDBLU_AMBIENT_LIGHT,    /*!< */
  STM_PANEL_LEDBLU_SCANNING_BLU,     /*!< */
} stm_display_panel_ledblu_range_sel_t;


/*! \struct  stm_display_panel_ledblu_range_s
 *  \brief   LED backlight configuration value range query
 *  \ctrlarg OUTPUT_CTRL_PANEL_READ_CTRL_RANGE
 *  \apis    ::stm_display_output_get_compound_control()
 */
typedef struct stm_display_panel_ledblu_range_s
{
  stm_display_panel_ledblu_range_sel_t selector;  /*!< */
  uint16_t                             value_min; /*!< */
  uint16_t                             value_max; /*!< */
} stm_display_panel_ledblu_range_t;


#if defined(__cplusplus)
}
#endif

#endif /* _STM_LVDS_OUTPUT_H */
