/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_ifm.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_IFM_H__
#define __HDMIRX_IFM_H__

#include <stddefs.h>
#include <stm_hdmirx.h>

typedef struct
{
  U16 Hstart;
  U16 Width;
  U16 Vstart;
  U16 Length;
} sthdmirx_timing_window_param_t;

typedef struct
{
  U16 HTotal;
  U16 VTotal;
  sthdmirx_timing_window_param_t Window;
} sthdmirx_sig_timing_info_t;

typedef enum
{
  STHDRX_IBD_MEASURE_SOURCE,
  STHDRX_IBD_MEASURE_WINDOW,
  STHDRX_IBD_RGB_SELECT,
  STHDRX_IBD_THRESHOLD,
  STHDRX_IBD_ENABLE,
} sthdmirx_IBD_ctrl_option_t;

enum
{
  STHDRX_IBD_MEASURE_DATA,
  STHDRX_IBD_MEASURE_DE
};

enum
{
  STHDRX_IBD_RGB_FIELD_SEL,
  STHDRX_IBD_RED_FIELD_SEL,
  STHDRX_IBD_GREEN_FIELD_SEL,
  STHDRX_IBD_BLUE_FIELD_SEL
};

typedef struct
{
  U32 ulBaseAddress;
  U32 ulMeasTCLK_Hz;
  BOOL bIsAWDCaptureEnable;
} sthdmirx_IFM_context_t;

typedef struct
{
  U16 HPeriod;
  U16 VPeriod;
  U16 HPulse;
  U16 VPulse;
  U32 HFreq_Hz;
  U16 VFreq_Hz;
  stm_hdmirx_signal_polarity_t H_SyncPolarity;
  stm_hdmirx_signal_polarity_t V_SyncPolarity;
} sthdmirx_IFM_timing_params_t;

void sthdmirx_IFM_instrument_initialize(sthdmirx_IFM_context_t *pIfmCtrl);
void sthdmirx_IFM_init(sthdmirx_IFM_context_t *pIfmCtrl);
BOOL sthdmirx_IFM_signal_timing_get(sthdmirx_IFM_context_t *pIfmCtrl,
                                    sthdmirx_IFM_timing_params_t *pIfmParams);
void sthdmirx_IFM_active_window_set(sthdmirx_IFM_context_t *pIfmCtrl,
                                    sthdmirx_timing_window_param_t *mWindow);
void sthdmirx_IFM_IBD_meas_window_set(sthdmirx_IFM_context_t *pIfmCtrl,
                                      sthdmirx_timing_window_param_t *mWindow);
void sthdmirx_IFM_IBD_config(sthdmirx_IFM_context_t *pIfmCtrl,
                             sthdmirx_IBD_ctrl_option_t Option, U8 uValue);
BOOL sthdmirx_IFM_IBD_timing_get(sthdmirx_IFM_context_t *pIfmCtrl,
                                 sthdmirx_sig_timing_info_t *pIbdParam);
stm_hdmirx_signal_polarity_t
sthdmirx_IFM_HSync_polarity_get(sthdmirx_IFM_context_t *IFM_Control);
stm_hdmirx_signal_polarity_t
sthdmirx_IFM_VSync_polarity_get(sthdmirx_IFM_context_t *IFM_Control);
U32 sthdmirx_IFM_pixel_clk_freq_get(sthdmirx_IFM_context_t *pIfmCtrl);
stm_hdmirx_signal_scan_type_t
sthdmirx_IFM_signal_scantype_get(sthdmirx_IFM_context_t *pIfmCtrl);
BOOL sthdmirx_IFM_get_AFRsignal_detect_status(
  sthdmirx_IFM_context_t *pIfmCtrl);
void sthdmirx_IFM_clear_AFRsignal_detect_status(
  sthdmirx_IFM_context_t *pIfmCtrl);
void sthdmirx_IFM_clear_interlace_decode_error_status(
  sthdmirx_IFM_context_t *pIfmCtrl);
BOOL sthdmirx_IFM_waitVSync_time(sthdmirx_IFM_context_t *pIfmCtrl,
                                 U8 NoOfVsyncs);
BOOL sthdmirx_IFM_is_mode_overlapping(U16 Hfreq, U16 Vfreq);
void sthdmirx_IFM_set_australianmode_interlace_detect(
  sthdmirx_IFM_context_t *pIfmCtrl);
void sthdmirx_IFM_reset_australianmode_interlace_detect(
  sthdmirx_IFM_context_t *pIfmCtrl);

#endif /*end of __HDMIRX_IFM_H__ */
