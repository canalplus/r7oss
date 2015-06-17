/***********************************************************************
 *
 * File: include/stm_analog_output.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_ANALOG_OUTPUT_H
#define _STM_ANALOG_OUTPUT_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_analog_output.h
 *  \brief type definitions for analog output specific controls.
 */


/*! \enum    stm_chroma_delay_e
 *  \brief   Valid chroma/luma pixel delay values for the OUTPUT_CTRL_CHROMA_DELAY control.
 *  \ctrlarg OUTPUT_CTRL_CHROMA_DELAY
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 *
 */
typedef enum stm_chroma_delay_e
{
  STM_CHROMA_DELAY_M_2_0, /*!< -2.0 pixels */
  STM_CHROMA_DELAY_M_1_5, /*!< -1.5 pixels */
  STM_CHROMA_DELAY_M_1_0, /*!< -1.0 pixels */
  STM_CHROMA_DELAY_M_0_5, /*!< -0.5 pixels */
  STM_CHROMA_DELAY_0,     /*!< No delay    */
  STM_CHROMA_DELAY_0_5,   /*!< +0.5 pixels */
  STM_CHROMA_DELAY_1_0,   /*!< +1.0 pixels */
  STM_CHROMA_DELAY_1_5,   /*!< +1.5 pixels */
  STM_CHROMA_DELAY_2_0,   /*!< +2.0 pixels */
  STM_CHROMA_DELAY_2_5    /*!< +2.5 pixels */
} stm_chroma_delay_t;


/*! \enum    stm_teletext_system_e
 *  \brief   Teletext system configuration for hardware teletext insertion
 *  \ctrlarg OUTPUT_CTRL_TELETEXT_SYSTEM
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 *
 */
typedef enum stm_teletext_system_e
{
  STM_TELETEXT_SYSTEM_A = 0, /*!< */
  STM_TELETEXT_SYSTEM_B,     /*!< */
  STM_TELETEXT_SYSTEM_C,     /*!< */
  STM_TELETEXT_SYSTEM_D      /*!< */
} stm_teletext_system_t;


/***************************************************************************
 * What follows are hardware specific definitions for filters in different
 * parts of the analogue output implementations. They are made available so
 * customers can set board specific filter coefficients, overriding the ST
 * defaults, on instruction from ST.
 */

/*! \enum stm_filter_coeff_types_e
 *  \brief supported filter coefficient target types
 *  \ctrlarg OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef enum stm_filter_coeff_types_e
{
  HDF_COEFF_2X_LUMA,       /*!< HD DAC HD Luma/RGB filter (SRC 2x)             */
  HDF_COEFF_2X_CHROMA,     /*!< HD DAC HD Chroma filter (SRC 2x)               */
  HDF_COEFF_2X_ALT_LUMA,   /*!< Alternative HD DAC HD Luma/RGB filter (SRC 2x) */
  HDF_COEFF_2X_ALT_CHROMA, /*!< Alternative HD DAC HD Chroma filter (SRC 2x)   */
  HDF_COEFF_4X_LUMA,       /*!< HD DAC SD/ED Luma/RGB filter (SRC 4x)          */
  HDF_COEFF_4X_CHROMA,     /*!< HD DAC SD/ED Chroma filter (SRC 4x)            */
  DENC_COEFF_LUMA,         /*!< DENC (SD DAC) Luma/RGB filter                  */
  DENC_COEFF_CHROMA,       /*!< DENC (SD DAC) Chroma filter                    */
} stm_filter_coeff_types_t;


/*! \enum stm_filter_divide_e
 *  \brief Filter divider values
 *  \ctrlarg OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef enum stm_filter_divide_e
{
  STM_FLT_DIV_256 = 0, /*!< */
  STM_FLT_DIV_512,     /*!< */
  STM_FLT_DIV_1024,    /*!< */
  STM_FLT_DIV_2048,    /*!< */
  STM_FLT_DIV_4096,    /*!< */
  STM_FLT_DISABLED     /*!< */
} stm_filter_divide_t;


/*! \enum stm_analog_factors_type_e
 *  \brief supported analog calibration factors target types
 *  \ctrlarg OUTPUT_CTRL_VIDEO_OUT_CALIBRATION
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef enum stm_analog_factors_type_e
{
  DENC_FACTORS = (1L<<0),   /*!< DENC (SD DAC) calibration factors  */
  HDF_FACTORS  = (1L<<1),   /*!< HDF (HD DAC) calibration factors   */
} stm_analog_factors_type_t;


/*! \struct stm_display_denc_filter_setup_s
 *  \brief Filter setup for DENC (SD DAC) Luma and Chroma filters
 *  \ctrlarg OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef struct stm_display_denc_filter_setup_s
{
  stm_filter_divide_t      div;       /*!< */
  int32_t                  coeff[10]; /*!< */
} stm_display_denc_filter_setup_t;

/*! \struct stm_display_vps_s
 *  \brief Write and enable/disable Video Program System data on DENC
 *  \ctrlarg OUTPUT_CTRL_VPS
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef struct stm_display_vps_s
{
  bool    vps_enable;  /*!< */
  uint8_t vps_data[6]; /*!< */
} stm_display_vps_t;

/*! \struct stm_display_hdf_filter_setup_s
 *  \brief Filter setup for DAC DAC Luma and Chroma filters
 *  \ctrlarg OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef struct stm_display_hdf_filter_setup_s
{
  stm_filter_divide_t      div;      /*!< */
  uint32_t                 coeff[9]; /*!< */
} stm_display_hdf_filter_setup_t;


/*! \struct stm_display_filter_setup_s
 *  \brief Filter setup descriptor
 *  \ctrlarg OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef struct stm_display_filter_setup_s
{
  stm_filter_coeff_types_t type;          /*!< */
  union {
    stm_display_denc_filter_setup_t denc; /*!< */
    stm_display_hdf_filter_setup_t  hdf;  /*!< */
  };
} stm_display_filter_setup_t;


/*! \struct stm_display_analog_factors_s
 *  \brief Analog Sync factors descriptor
 *  \ctrlarg OUTPUT_CTRL_VIDEO_OUT_CALIBRATION
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef struct stm_display_analog_factors_s
{
    uint16_t                    Scaling_factor_Cb;  /*!< */
    uint16_t                    Scaling_factor_Y;   /*!< */
    uint16_t                    Scaling_factor_Cr;  /*!< */
    short int                   Offset_factor_Cb;   /*!< */
    short int                   Offset_factor_Y;    /*!< */
    short int                   Offset_factor_Cr;   /*!< */
} stm_display_analog_factors_t;


/*! \struct stm_display_analog_calibration_setup_s
 *  \brief Analog sync calibration setup
 *  \ctrlarg OUTPUT_CTRL_VIDEO_OUT_CALIBRATION
 *  \apis ::stm_display_output_set_compound_control()
 */
typedef struct stm_display_analog_calibration_setup_s
{
    uint32_t                        type;       /*!< */
    stm_display_analog_factors_t    denc;       /*!< */
    stm_display_analog_factors_t    hdf;        /*!< */
} stm_display_analog_calibration_setup_t;

#if defined(__cplusplus)
}
#endif

#endif /* _STM_ANALOG_OUTPUT_H */
