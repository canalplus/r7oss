/***********************************************************************
 *
 * File: hdmirx/src/phy/hdmirx_Combophy.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_ANALOGPHY_H__
#define __HDMIRX_ANALOGPHY_H__

/*Includes------------------------------------------------------------------------------*/
#include <stddefs.h>

/* Support dual Interface C & C++*/
#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/
  typedef enum {
    HDMIRX_STANDARD_ANALOG_PHY,
    HDMIRX_COMBO_ANALOG_PHY
  }
  sthdmirx_PHY_cell_design_type_t;

  typedef enum
  {
    SIGNAL_EQUALIZATION_DISABLED,
    SIGNAL_EQUALIZATION_ADAPTIVE,
    SIGNAL_EQUALIZATION_LOWGAIN,
    SIGNAL_EQUALIZATION_MIDGAIN,
    SIGNAL_EQUALIZATION_HIGHGAIN,
    SIGNAL_EQUALIZATION_CUSTOM,
  } sthdmirx_PHY_signl_EQ_scheme_type_t;

  typedef enum
  {
    EQ_STATE_IDLE,
    EQ_STATE_RUN,
//  EQ_STATE_COARSE_SEARCH,
    //EQ_STATE_FINE_SEACRH,
    EQ_STATE_DONE,
    EQ_STATE_FAIL
  } sthdmirx_PHY_equalizer_state_t;

  typedef enum
  {
    RTERM_CALIBRATION_MANUAL,
    RTERM_CALIBRATION_AUTO
  } sthdmirx_phy_Rterm_calibration_mode_t;

  typedef enum
  {
    RESET_TCLK_DOMAIN = 0x01,
    RESET_LCLK_DOMAIN = 0x02,
    RESET_OVERSAMPLE_CLK_DOMIAN = 0x04,
    PHY_RESET_ALL =
    (RESET_TCLK_DOMAIN | RESET_LCLK_DOMAIN |RESET_OVERSAMPLE_CLK_DOMIAN)
  } sthdmirx_PHY_reset_domain_type_t;

  typedef enum
  {
    LCLK_SEL_GND,
    LCLK_SEL_OVERSAMPLED_CLK,
    LCLK_SEL_TCLK
  } sthdmirx_PHY_clk_selection_t;

  typedef enum
  {
    INVERSE_LCLK,
    INVERSE_OVR_SAMPLE_CLK
  } sthdmirx_PHY_sel_clk_inverse_t;

  /* don't change the enum order, written as per hardware*/
  typedef enum
  {
    PHY_POWER_ON = 0x0,
    PHY_POWERMODE_RSVD,
    PHY_STANDBY = 0x02,
    PHY_POWER_OFF = 0x03
  } sthdmirx_PHY_pwr_modes_t;

  typedef enum
  {
    PHY_ANALOG_REFERENCE_CLOCK_PRESENT,
    PHY_OVR_SAMPLE_LINK_CLOCK_PRESENT,
    PHY_REGENERATED_LINK_CLOCK_PRESENT,
    PHY_ACTIVE_SOURCE_SELECTED_BY_PHY,
    PHY_CURRENT_ACTIVE_CHANNEL_NUMBER
  } sthdmirx_PHY_HW_status_t;

  /* Private Constants --------------------------------------------------------------------- */

  /* Private variables  --------------------------------------------------------------- */

  /* Global Variables ----------------------------------------------------------------------- */
  typedef struct
  {
    unsigned int DviEq:4;	/* adjust the frequency response */
    unsigned int EqGain:2;	/* Adjust the gain of Equalizer */
    unsigned int AutoOffset:1;	/* Auto Equalizer Offset enable/disabel */
    unsigned int Rsvd:1;	/* Reserved for Future purpose */
  } equalizer_t;

  typedef struct
  {
    U32 LowFreqGain;
    U32 HighFreqGain;
  } customEQ_t;

  enum
  {
    EQ_LOW_FREQ_GAIN_0dB = 0,
    EQ_LOW_FREQ_GAIN_MINUS_2dB = 1,
    EQ_LOW_FREQ_GAIN_MINUS_4dB = 2,
    EQ_LOW_FREQ_GAIN_MINUS_6dB = 3,
    EQ_LOW_FREQ_GAIN_MAX_NUM = 4
  };

  typedef struct
  {
    sthdmirx_PHY_equalizer_state_t EqState;
    unsigned int MarkedBestEqCandidate;
  } sthdmirx_adaptiveEQ_t;

  typedef struct
  {
    sthdmirx_PHY_cell_design_type_t PhyCellType;
    sthdmirx_PHY_signl_EQ_scheme_type_t EqualizationType;
    sthdmirx_phy_Rterm_calibration_mode_t RTermCalibrationMode;
    U8 RTermValue;
    sthdmirx_adaptiveEQ_t AdaptiveEqHandle;
    equalizer_t DefaultEqSetting;
    U32 BaseAddress_p;
    customEQ_t CustomEqSetting;
    U32 DeviceBaseAddress;
  } sthdmirx_PHY_context_t;

  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/
  void sthdmirx_PHY_init(sthdmirx_PHY_context_t *PhyControl_p);
  void sthdmirx_PHY_PLL_setup(sthdmirx_PHY_context_t *PhyControl_p,
                              U32 LinkClkFreq, BOOL IsPLLInit);
  void sthdmirx_PHY_set_eq_gain(sthdmirx_PHY_context_t *PhyControl_p,
                                U8 EqValue);
  void sthdmirx_PHY_set_pk_freq(sthdmirx_PHY_context_t *PhyControl_p,
                                U8 PkValue);
  void sthdmirx_PHY_Rterm_calibration(sthdmirx_PHY_context_t *PhyControl_p,
                                      sthdmirx_phy_Rterm_calibration_mode_t rTermMode, U8 rTermValue);
  void sthdmirx_PHY_clk_selection(sthdmirx_PHY_context_t *PhyControl_p,
                                  sthdmirx_PHY_clk_selection_t clk);
  void sthdmirx_PHY_clk_inverse(sthdmirx_PHY_context_t *PhyControl_p,
                                sthdmirx_PHY_sel_clk_inverse_t clk,U8 Inverse);
  void sthdmirx_PHY_power_mode_setup(sthdmirx_PHY_context_t *PhyControl_p,
                                     sthdmirx_PHY_pwr_modes_t pMode);
  void sthdmirx_PHY_set_fast_clk_meas(sthdmirx_PHY_context_t *PhyControl_p,
                                      U8 fmCtrl,U8 mDuration);
  void sthdmirx_PHY_clear_sigqual_score(
    sthdmirx_PHY_context_t *PhyControl_p);
  U32 sthdmirx_PHY_get_signalquality_score(
    sthdmirx_PHY_context_t *PhyControl_p,U32 uDurationMsec);
  U8 sthdmirx_PHY_get_phy_status(sthdmirx_PHY_context_t *PhyControl_p,
                                 sthdmirx_PHY_HW_status_t CheckStatus);
  void sthdmirx_PHY_setup_equalization_scheme(
    sthdmirx_PHY_context_t *PhyControl_p);
  /* ------------------------------- End of file ---------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_ANALOGPHY_H__ */
