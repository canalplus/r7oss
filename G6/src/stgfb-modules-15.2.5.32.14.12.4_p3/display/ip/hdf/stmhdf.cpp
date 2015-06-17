/***********************************************************************
 *
 * File: display/ip/hdf/stmhdf.cpp
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/Output.h>
#include <display/generic/DisplayDevice.h>

#include "stmhdf.h"
#include "stmhdfreg.h"


//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTmHDFormatter::CSTmHDFormatter(
    CDisplayDevice      *pDev,
    uint32_t             TVFmt)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDevRegs   = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_TVFmt      = TVFmt;
  m_pOwner     = 0;
  m_pHDFSync   = 0;

  m_bHasSeparateCbCrRescale     = true;
  m_bHas4TapSyncFilter          = false;
  m_bUseAlternate2XFilter       = false;

  vibe_os_zero_memory(&m_CurrentMode, sizeof(stm_display_mode_t));
  m_CurrentMode.mode_id         = STM_TIMING_MODE_RESERVED;

  m_bMainInput                  = true;
  m_ulInputFormat               = 0;
  m_signalRange                 = STM_SIGNAL_FILTER_SAV_EAV;

  m_AWG_Y_C_Offset              = 0;
  vibe_os_zero_memory(&m_pHDFParams, sizeof(HDFParams_t));
  vibe_os_zero_memory(&m_filters, sizeof(stm_display_hdf_filter_setup_t));

  /*
   * Ensure the ANC AWG is initially disabled.
   */
  WriteHDFReg(HDF_ANA_ANCILIARY_CTRL,0);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmHDFormatter::~CSTmHDFormatter()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  delete m_pHDFSync;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDFormatter::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pHDFSync = new CSTmHDFSync("hdf");
  if(!m_pHDFSync || !m_pHDFSync->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create HDF sync calibration object" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmHDFormatter::Start(const COutput *pOwner, const stm_display_mode_t *mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(IsStarted() && (m_pOwner != pOwner))
  {
    TRC( TRC_ID_ERROR, "CSTmHDFormatter::Start - HDFormatter is already started by another output!!" );
    return false;
  }

  m_pOwner       = pOwner;
  m_CurrentMode  = *mode;

  TRC( TRC_ID_MAIN_INFO, "CSTmHDFormatter::Start - HDFormatter is Started: Owner=%p, Mode ID=%d", m_pOwner, m_CurrentMode.mode_id );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDFormatter::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  SetUpsampler(0);

  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
  m_bMainInput          = true;
  m_ulInputFormat       = 0;
  m_signalRange         = STM_SIGNAL_FILTER_SAV_EAV;

  m_pOwner              = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDFormatter::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  /*
   * Ensure the ANC AWG is initially disabled.
   *
   * Note: Restoring the state of this after a suspend is out of the scope
   *       of this class and is handled externally.
   */
  WriteHDFReg(HDF_ANA_ANCILIARY_CTRL,0);

  /*
   * Ensure control state is put back, but leave the formatter essentially
   * disabled. The idea is not to restore the full dynamic state that
   * existed before the suspend, but to put it back in a state where the
   * owning output can call the startup sequence again.
   *
   * Note: subclasses will override this to do some IP version specific
   *       setup.
   */
  SetSignalRangeClipping();
  SetUpsampler(0);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDFormatter::SetAWGYCOffset(const uint32_t ulY_C_Offset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_AWG_Y_C_Offset = ulY_C_Offset;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


uint32_t CSTmHDFormatter::SetControl(stm_output_control_t ctrl, uint32_t val)
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      m_signalRange = (stm_display_signal_range_t)val;
      SetSignalRangeClipping();
      break;
    }
    case OUTPUT_CTRL_DAC_HD_ALT_FILTER:
    {
      m_bUseAlternate2XFilter = (val != 0);
      if(IsStarted() && (m_CurrentMode.mode_params.output_standards & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA)))
      {
        if(m_CurrentMode.mode_timing.pixel_clock_freq <= 74250000)
          SetUpsampler(2);
      }
      break;
    }
    default:
      break;
  }

  return STM_OUT_OK;
}


uint32_t CSTmHDFormatter::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
      *val = (uint32_t)m_signalRange;
      break;
    case OUTPUT_CTRL_DAC_HD_ALT_FILTER:
      *val = m_bUseAlternate2XFilter;
      break;
    default:
      break;
  }

  return STM_OUT_OK;
}


uint32_t CSTmHDFormatter::SetCompoundControl(stm_output_control_t ctrl, void *newVal)
{
  uint32_t ret = STM_OUT_NO_CTRL;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
    {
      bool RetOk = true;
      const stm_display_analog_calibration_setup_t *f = (stm_display_analog_calibration_setup_t *)newVal;
      if(f->type & HDF_FACTORS)
      {
        RetOk = m_pHDFSync->SetCalibrationValues(m_CurrentMode, &f->hdf);
        if(RetOk)
        {
          if(((m_ulInputFormat & STM_VIDEO_OUT_RGB) == 0) &&
             ((m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK) == 0))
          {
            DisableAWG();
          }
          ProgramYCbCrReScalers();
        }
      }
      ret = RetOk?STM_OUT_OK:STM_OUT_INVALID_VALUE;
      break;
    }
    default:
      break;
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


uint32_t CSTmHDFormatter::GetCompoundControl(stm_output_control_t ctrl, void *val) const
{
  uint32_t ret = STM_OUT_NO_CTRL;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
    {
      bool RetOk = false;
      stm_display_analog_calibration_setup_t *f = (stm_display_analog_calibration_setup_t *)val;
      vibe_os_zero_memory(&f->hdf, sizeof(stm_display_analog_factors_t));
      RetOk = m_pHDFSync->GetCalibrationValues(&f->hdf);
      if(RetOk)
        f->type |= HDF_FACTORS;
      ret = RetOk?STM_OUT_OK:STM_OUT_INVALID_VALUE;
      break;
    }
    default:
      break;
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


bool CSTmHDFormatter::SetOutputFormat(uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!IsStarted())
  {
    TRC( TRC_ID_ERROR, "HDFormatter Not yet Started !!" );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "- format = 0x%x", format );

  if(format != m_ulInputFormat)
  {
    TRC( TRC_ID_ERROR, "Trying to set a different format as InputFormat !!" );
    return false;
  }

  /*
   * It may seem strange that this is in SetFormat rather than the Start
   * methods, but it is to cope with the dynamic switching of the HD dacs
   * between the main and aux display pipelines.
   */
  if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_NTG5)
  {
    /*
     * Digital only, so put the DAC SRC into bypass
     */
    SetUpsampler(1);
  }
  else if(m_CurrentMode.mode_params.output_standards & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
  {
    if(m_CurrentMode.mode_timing.pixel_clock_freq > 74250000)
      SetUpsampler(1);
    else
      SetUpsampler(2);
  }
  else
  {
    SetUpsampler(4);
  }

  /* Disable AWG */
  DisableAWG();

  ASSERTF((m_pHDFSync != 0),("HDFormatter class has not been created correctly\n"));

  m_pHDFSync->SetCalibrationValues(m_CurrentMode);

  ProgramYCbCrReScalers();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDFormatter::ProgramYCbCrReScalers(void)
  {
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pHDFSync->GetHWConfiguration(m_ulInputFormat, &m_pHDFParams))
  {

    /*
     * Do not start embedded syncs for RGB or any SD output, we just want the
     * hardware scaler values.
     */
    if(((m_ulInputFormat & STM_VIDEO_OUT_RGB) == 0) &&
       ((m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK) == 0))
    {
      EnableAWG();
    }
  }
  else
  {
    /*
     * No firmware loaded or available for this mode (e.g. 1080p50/60, VGA etc),
     * so scale the analog signal to 0, except for SD modes coming from the DENC.
     * In that case we put the rescale to 100% and offset to 0, assuming that
     * the DENC will be scaling the signal before it gets to the HD Formatter.
     *
     * Note: Previous versions of the driver supported VESA modes output in RGB
     *       with no syncs (required external sync signals) correctly scaled
     *       and suitable for an analog PC monitor D-Sub connector. The
     *       validation provided analog sync calibration now used does not
     *       provide for this use case, no analog signal will be output in
     *       these modes.
     *
     *       Also previous versions of the analog sync firmware provided
     *       with the driver in the STLinux distribution included embedded syncs
     *       for 1080p50/60 modes. Although these did basically work and the
     *       HD DACs run at 148.5MHz, we are told they are not designed to
     *       provide the correct signal frequency response with 148.5MHz input,
     *       only 74.25MHz input upsampled by 2x. Therefore the validation
     *       provided analog sync calibration do not support these modes and no
     *       analog signal will be output.
     */

    if((m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK) != 0)
    {
      m_pHDFParams.ANA_SCALE_CTRL_DAC_Y  = 0x400;
      m_pHDFParams.ANA_SCALE_CTRL_DAC_Cb = 0x400;
      m_pHDFParams.ANA_SCALE_CTRL_DAC_Cr = 0x400;
    }
    else
    {
      m_pHDFParams.ANA_SCALE_CTRL_DAC_Y  = 0x0;
      m_pHDFParams.ANA_SCALE_CTRL_DAC_Cb = 0x0;
      m_pHDFParams.ANA_SCALE_CTRL_DAC_Cr = 0x0;
    }
  }

  TRC( TRC_ID_MAIN_INFO, "CSTmHDFormatter::ProgramYCbCrReScalers: [0x%x,0x%x,0x%x]", (uint32_t)m_pHDFParams.ANA_SCALE_CTRL_DAC_Cb , (uint32_t)m_pHDFParams.ANA_SCALE_CTRL_DAC_Y , (uint32_t)m_pHDFParams.ANA_SCALE_CTRL_DAC_Cr );

  /* Program ReScalers */
  SetYCbCrReScalers();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDFormatter::SetFilterCoefficients(const stm_display_filter_setup_t *f)
{
  bool ret;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(f->hdf.div < STM_FLT_DIV_4096)
  {
    TRC( TRC_ID_MAIN_INFO, "Setting filter %d",f->type );
    m_filters[f->type] = f->hdf;
    ret = true;
  }
  else
  {
    TRC( TRC_ID_ERROR, "Unsupported filter divide %d",f->hdf.div );
    ret = false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


void CSTmHDFormatter::SetUpsampler(const int multiple)
{
  uint32_t val,srccfg;
  stm_filter_coeff_types_t luma,chroma;
  bool isRGB = (m_ulInputFormat & STM_VIDEO_OUT_RGB) != 0;

  switch(multiple)
  {
    case 2:
      if(!m_bUseAlternate2XFilter)
      {
        TRC( TRC_ID_MAIN_INFO, "2X" );
        luma   = HDF_COEFF_2X_LUMA;
        chroma = isRGB?HDF_COEFF_2X_LUMA:HDF_COEFF_2X_CHROMA;
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "2X (ALT)" );
        luma   = HDF_COEFF_2X_ALT_LUMA;
        chroma = isRGB?HDF_COEFF_2X_ALT_LUMA:HDF_COEFF_2X_ALT_CHROMA;
      }
      srccfg = ANA_SRC_CFG_2X;
      break;
    case 4:
      TRC( TRC_ID_MAIN_INFO, "4X" );
      luma   = HDF_COEFF_4X_LUMA;
      chroma = isRGB?HDF_COEFF_4X_LUMA:HDF_COEFF_4X_CHROMA;
      srccfg = ANA_SRC_CFG_4X;
      break;
    case 1:
    default:
      TRC( TRC_ID_MAIN_INFO, "disabled/bypass" );
      WriteHDFReg(HDF_LUMA_SRC_CFG, (ANA_SRC_CFG_DISABLE|ANA_SRC_CFG_DIV_512|ANA_SRC_CFG_BYPASS));
      WriteHDFReg(HDF_CHROMA_SRC_CFG, ANA_SRC_CFG_DIV_512);
      return;
  }

  TRC( TRC_ID_MAIN_INFO, "luma filter index = %d chroma filter index = %d", luma, chroma );

  val = srccfg | (m_filters[luma].coeff[HDF_COEFF_PHASE1_7] << 16) |
                 (m_filters[luma].div << ANA_SRC_CFG_DIV_SHIFT);

  WriteHDFReg(HDF_LUMA_SRC_CFG, val);

  val = srccfg | (m_filters[chroma].coeff[HDF_COEFF_PHASE1_7] << 16) |
                 (m_filters[chroma].div << ANA_SRC_CFG_DIV_SHIFT);

  WriteHDFReg(HDF_CHROMA_SRC_CFG, val);

  TRC( TRC_ID_UNCLASSIFIED, "Luma P1: 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE1_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE1_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE1_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE1_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE1_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE1_456]>>19)&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE1_7]&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, "Luma P2: 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE2_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE2_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE2_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE2_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE2_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE2_456]>>19)&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, "Luma P3: 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE3_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE3_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE3_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE3_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE3_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE3_456]>>19)&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, "Luma P4: 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE4_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE4_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE4_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[luma].coeff[HDF_COEFF_PHASE4_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE4_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[luma].coeff[HDF_COEFF_PHASE4_456]>>19)&0x3ff );

  WriteHDFReg(HDF_LUMA_COEFF_P1_T123, m_filters[luma].coeff[HDF_COEFF_PHASE1_123]);
  WriteHDFReg(HDF_LUMA_COEFF_P1_T456, m_filters[luma].coeff[HDF_COEFF_PHASE1_456]);
  WriteHDFReg(HDF_LUMA_COEFF_P2_T123, m_filters[luma].coeff[HDF_COEFF_PHASE2_123]);
  WriteHDFReg(HDF_LUMA_COEFF_P2_T456, m_filters[luma].coeff[HDF_COEFF_PHASE2_456]);
  WriteHDFReg(HDF_LUMA_COEFF_P3_T123, m_filters[luma].coeff[HDF_COEFF_PHASE3_123]);
  WriteHDFReg(HDF_LUMA_COEFF_P3_T456, m_filters[luma].coeff[HDF_COEFF_PHASE3_456]);
  WriteHDFReg(HDF_LUMA_COEFF_P4_T123, m_filters[luma].coeff[HDF_COEFF_PHASE4_123]);
  WriteHDFReg(HDF_LUMA_COEFF_P4_T456, m_filters[luma].coeff[HDF_COEFF_PHASE4_456]);

  TRC( TRC_ID_UNCLASSIFIED, "Chroma P1: 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]>>19)&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE1_7]&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, "Chroma P2: 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]>>19)&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, "Chroma P3: 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]>>19)&0x3ff );

  TRC( TRC_ID_UNCLASSIFIED, "Chroma P4: 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]&0x7f );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]>>7)&0xff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]>>15)&0x1ff );

  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]&0x1ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]>>9)&0x3ff );
  TRC( TRC_ID_UNCLASSIFIED, " 0x%x", (m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]>>19)&0x3ff );

  WriteHDFReg(HDF_CHROMA_COEFF_P1_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]);
  WriteHDFReg(HDF_CHROMA_COEFF_P1_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]);
  WriteHDFReg(HDF_CHROMA_COEFF_P2_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]);
  WriteHDFReg(HDF_CHROMA_COEFF_P2_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]);
  WriteHDFReg(HDF_CHROMA_COEFF_P3_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]);
  WriteHDFReg(HDF_CHROMA_COEFF_P3_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]);
  WriteHDFReg(HDF_CHROMA_COEFF_P4_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]);
  WriteHDFReg(HDF_CHROMA_COEFF_P4_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]);
}
