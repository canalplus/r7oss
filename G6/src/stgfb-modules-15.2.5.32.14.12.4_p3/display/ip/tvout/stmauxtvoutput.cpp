/***********************************************************************
 *
 * file: display/ip/tvout/stmauxtvoutput.cpp
 * Copyright (c) 2009-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/DisplayMixer.h>

#include <display/ip/stmdenc.h>
#include <display/ip/displaytiming/stmvtg.h>
#include <display/ip/hdf/stmhdf.h>

#include "stmauxtvoutput.h"

//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on Aux pipeline
//
CSTmAuxTVOutput::CSTmAuxTVOutput(
    const char            *name,
    uint32_t               id,
    CDisplayDevice        *pDev,
    CSTmVTG               *pVTG,
    CSTmDENC              *pDENC,
    CDisplayMixer         *pMixer,
    CSTmHDFormatter       *pHDFormatter): CSTmMasterOutput(name, id, pDev, pDENC, pVTG, pMixer)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_ulCapabilities |= (OUTPUT_CAPS_SD_ANALOG             |
                       OUTPUT_CAPS_ED_ANALOG             |
                       OUTPUT_CAPS_HD_ANALOG             |
                       OUTPUT_CAPS_EXTERNAL_SYNC_SIGNALS |
                       OUTPUT_CAPS_CVBS_YC_EXCLUSIVE     |
                       OUTPUT_CAPS_YPbPr_EXCLUSIVE       |
                       OUTPUT_CAPS_RGB_EXCLUSIVE         |
                       OUTPUT_CAPS_SD_RGB_CVBS_YC        |
                       OUTPUT_CAPS_SD_YPbPr_CVBS_YC);

  m_pHDFormatter = pHDFormatter;
  m_uExternalSyncShift = 0;
  m_bInvertExternalSyncs = false;
  m_uDENCSyncOffset = 0;

  TRC( TRC_ID_MAIN_INFO, "- m_pVTG          = %p", m_pVTG );
  TRC( TRC_ID_MAIN_INFO, "- m_pDENC         = %p", m_pDENC );
  TRC( TRC_ID_MAIN_INFO, "- m_pMixer        = %p", m_pMixer );
  TRC( TRC_ID_MAIN_INFO, "- m_pHDFFormatter = %p", m_pHDFormatter );

  /* Set default Output format */
  m_ulOutputFormat = (STM_VIDEO_OUT_CVBS | STM_VIDEO_OUT_YC);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmAuxTVOutput::~CSTmAuxTVOutput() {}


const stm_display_mode_t* CSTmAuxTVOutput::SupportedMode(const stm_display_mode_t *mode) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if ((m_ulOutputFormat&(STM_VIDEO_OUT_CVBS | STM_VIDEO_OUT_YC))
      && !(mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
      /*
       * check mode is not a CEA861 pixel repeated SD mode.
       */
      && !((mode->mode_params.output_standards == STM_OUTPUT_STD_CEA861)
           &&(mode->mode_params.scan_type == STM_INTERLACED_SCAN)))
  {
    TRCOUT( TRC_ID_MAIN_INFO, "DENC in use, only SD modes are supported");
    return 0;
  }

  if (m_pHDFormatter &&((this == m_pHDFormatter->GetOwner())||(0 == m_pHDFormatter->GetOwner())))
  {
    if(mode->mode_params.output_standards & STM_OUTPUT_STD_NTG5)
    {
      TRC( TRC_ID_MAIN_INFO, "Looking for valid NTG5 mode, pixclock = %u", mode->mode_timing.pixel_clock_freq);
      return mode;
    }

    if(mode->mode_params.output_standards & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
    {
      TRC( TRC_ID_MAIN_INFO, "Looking for valid HD/XGA mode, pixclock = %u", mode->mode_timing.pixel_clock_freq);

      if(mode->mode_params.output_standards & STM_OUTPUT_STD_AS4933)
      {
        TRCOUT( TRC_ID_MAIN_INFO, "No support for AS4933 yet");
        return 0;
      }

      if(mode->mode_timing.pixel_clock_freq < 65000000 ||
         mode->mode_timing.pixel_clock_freq > 108108000)
      {
        TRC( TRC_ID_MAIN_INFO, "pixel clock out of range");
        goto check_slaved_outputs;
      }

      return mode;
    }

    /*
     * We support SD  progressive modes at 27Mhz or 27.027Mhz. We also report
     * support for SD interlaced modes over HDMI where the clock is doubled and
     * pixels are repeated. Finally we support VGA (640x480p@59.94Hz and 60Hz)
     * for digital displays (25.18MHz pixclock)
     */
    if(mode->mode_params.output_standards & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
    {
      TRC( TRC_ID_MAIN_INFO, "Looking for valid SD progressive mode");
      if(mode->mode_timing.pixel_clock_freq < 25174800 ||
         mode->mode_timing.pixel_clock_freq > 27027000)
      {
        TRC( TRC_ID_MAIN_INFO, "pixel clock out of range");
        goto check_slaved_outputs;
      }

      return mode;
    }
  }


  /*
   * We support SD interlaced modes based on a 13.5Mhz pixel clock
   */
  if(mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
  {
    TRC( TRC_ID_MAIN_INFO, "Looking for valid SD interlaced mode");
    if(mode->mode_timing.pixel_clock_freq < 13500000 ||
       mode->mode_timing.pixel_clock_freq > 13513500)
    {
      TRC( TRC_ID_MAIN_INFO, "pixel clock out of range , pixclock = %u\n", mode->mode_timing.pixel_clock_freq);
      goto check_slaved_outputs;
    }
    if(m_pDENC && ((m_pDENC->GetOwner() == this)||(m_pDENC->GetOwner() == 0)))
    {
      return mode;
    }
  }

check_slaved_outputs:
  TRC( TRC_ID_MAIN_INFO, "no matching mode found for tvout main, check slaved outputs");
  return CSTmMasterOutput::SupportedMode(mode);
}


OutputResults CSTmAuxTVOutput::Start(const stm_display_mode_t *mode)
{
  const stm_display_mode_t *supported;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* for non-custom modes, check if the standard is valid for that mode. */
  supported = GetModeParamsLine(mode->mode_id);
  if(supported)
  {
    stm_output_standards_e requested =
      static_cast<stm_output_standards_e>(mode->mode_params.output_standards);

    if (!(requested & supported->mode_params.output_standards))
    {
      TRC( TRC_ID_ERROR, "CSTmAuxTVOutput::Start - " );
      return STM_OUT_INVALID_VALUE;
    }
  }

  if(m_bIsStarted)
  {
    m_bPendingStart = true;
    Stop();
  }

  /*
   * Make sure we can use the VTG, on older SoCs it may be used by the main
   * path doing the "main to DENC" case as a slaved VTG.
   */
  if(m_pVTG->IsStarted())
  {
    TRC( TRC_ID_ERROR, "VTG required but it is in use by the main output" );
    return STM_OUT_BUSY;
  }

  /*
   * Determine if we are going to grab the DENC, we need to do this first
   * so subsequent logic depending on m_bUsingDENC will do the right thing.
   */
  if(mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
  {
    if(!m_pDENC->IsStarted())
      m_bUsingDENC = (m_ulOutputFormat != 0);
    if(m_bUsingDENC)
      TRC( TRC_ID_MAIN_INFO, "Using DENC" );
  }

  if(!SetVTGSyncs(mode))
  {
    TRC( TRC_ID_MAIN_INFO, "- failed" );
    return STM_OUT_INVALID_VALUE;
  }

  /*
   * Must do this before we start the VTGs and the FSynth otherwise
   * you get the wrong FSynth frequency for the clock dividers.
   */
  ConfigureDisplayClocks(mode);

  /*
   * Note: We call the base class Start() even when the output is already
   *       started to allow on-the-fly mode changes that require only the pixel
   *       clock frequency or DENC standard to be modified.
   */
  OutputResults retval=CSTmMasterOutput::Start(mode);

  if(retval != STM_OUT_OK)
  {
    TRC( TRC_ID_ERROR, "- failed to start master output" );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return retval;
}


bool CSTmAuxTVOutput::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Lock out interrupt processing while we test and shutdown the mixer, to
   * prevent a plane's vsync update processing enabling itself on
   * the mixer between the test and the Stop.
   */
  vibe_os_lock_resource(m_lock);

  // Disable all planes on the mixer.
  if(m_pMixer->HasEnabledPlanes())
  {
    TRC( TRC_ID_MAIN_INFO, "mixer has enabled planes" );
    m_pMixer->UpdateFromOutput(this, STM_PLANE_OUTPUT_STOPPED);
  }

  if(!m_bPendingStart)
  {
    // Stop slaved digital outputs.
    StopSlavedOutputs();
  }

  DisableDACs();

  // Stop HD Path.
  if(m_pHDFormatter->GetOwner() == this)
    m_pHDFormatter->Stop();

  // Stop SD Path.
  if(m_pDENC->GetOwner() == this)
    m_pDENC->Stop();

  m_bUsingDENC = false;

  m_pMixer->Stop();

  vibe_os_unlock_resource(m_lock);

  if(m_bIsStarted)
  {
    /*
     * If the output was properly running it is necessary to wait for vsyncs
     * in order to ensure the AWG has actually stopped. It also allows the
     * mixer enable register to latch 0 (all planes disabled) into the hardware.
     *
     * So we just busy wait here... but waiting for only one VSync is not always
     * enough, so we wait for 3 VSyncs :-(
     */
    for (int i = 0; i < 3; ++i)
    {
      stm_time64_t lastVSync = m_LastVTGEventTime;
      while (lastVSync == m_LastVTGEventTime)
        // would like to have some sort of cpu_relax() here
        ;
    }

    m_pVTG->Stop();

    vibe_os_lock_resource(m_lock);
    {
      m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
      COutput::Stop();
    }
    vibe_os_unlock_resource(m_lock);

  }

  DisableClocks();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmAuxTVOutput::RestartDENC(uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pDENC->IsStarted())
  {
    TRC( TRC_ID_ERROR, "DENC already in use !!" );
    /*
     * If the denc was previously started correctly then we should report an error here !!
     */
    return !m_bUsingDENC;
  }

  /*
   * Initially indicate that we are now using the DENC, so SetVTGSyncs
   * and ConfigureDisplayClocks will do the right thing.
   */
  m_bUsingDENC = true;

  TRC( TRC_ID_MAIN_INFO, "- output currently disabled, restarting DENC" );

  ConfigureDisplayClocks(&m_CurrentOutputMode);

  if(!m_pDENC->SetMainOutputFormat(format))
  {
    TRC( TRC_ID_ERROR, "Requested format (0x%x) not supported on the DENC",format );
    goto failed;
  }

  if(!m_pDENC->Start(this, &m_CurrentOutputMode))
  {
    TRC( TRC_ID_ERROR, "DENC start failed" );
    goto failed;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
failed:
  m_bUsingDENC = false;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return false;
}


/*
 * Helpers for SetOutputFormat below
 */
bool CSTmAuxTVOutput::SetOutputFormat(uint32_t format)
{
  TRC( TRC_ID_MAIN_INFO, "- format = 0x%x", format );

  /*
   * Don't change the output setup unless the display is running.
   */
  if(!m_bIsStarted)
  {
    TRC( TRC_ID_MAIN_INFO, "- No display mode, nothing to do" );
    goto exit;
  }

  if(!SetVTGSyncs(&m_CurrentOutputMode))
  {
    TRC( TRC_ID_ERROR, "SetVTGSyncs failed" );
    return false;
  }

  /* We support CVBS and YC formats only for SD Modes */
  if( (format & (STM_VIDEO_OUT_CVBS | STM_VIDEO_OUT_YC))
  && ((m_CurrentOutputMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK) == 0))
  {
    TRC( TRC_ID_ERROR, "CVBS/YC Modes are available only for SD Modes !!" );
    return false;
  }

  /*
   * Always stop the HD Formatter if we were currently using it.
   */
  if(m_pHDFormatter->GetOwner() == this)
      m_pHDFormatter->Stop();

  /*
   * Always stop the DENC if we were currently using it.
   */
  if(m_pDENC->GetOwner() == this)
    m_pDENC->Stop();

  if(format == 0) /* Disable Output */
  {
    DisableDACs();

    DisableClocks();

    return ConfigureOutput(format);
  }

  /* Restart DENC if SD modes */
  if(m_CurrentOutputMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
  {
    if(!RestartDENC(format))
      return false;
  }

  /*
   * Switch HD formatter for RGB(SCART) or YPrPb output from the aux pipeline
   */
  if((format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV)) == 0)
  {
    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return ConfigureOutput(format);
  }

  TRC( TRC_ID_MAIN_INFO, "- configuring HDFormatter" );

  if(m_pHDFormatter->IsStarted())
  {
    TRC( TRC_ID_ERROR, "HDFormatter is already in use !!" );
    return false;
  }

  /* Set Aux Clocks for HDFormatter */
  SetAuxClockToHDFormatter();

  /* Start HDFormatter */
  if(!m_pHDFormatter->Start(this, &m_CurrentOutputMode))
  {
    TRC( TRC_ID_ERROR, "HDFormatter Start Should Never Fail as we have already checked the owner" );
    return false;
  }

  /* Set AWG YC Offset */
  m_pHDFormatter->SetAWGYCOffset(m_AWG_Y_C_Offset);
  /* Set Denc source from Aux */
  m_pHDFormatter->SetDencSource(false);
  /* Set HDFormatter Input Format for Aux SCART */
  m_pHDFormatter->SetInputParams(false, format);
  /* Set HDFormatter OutputFormat */
  m_pHDFormatter->SetOutputFormat(format);

  if(!ConfigureOutput(format))
    return false;

exit:
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


uint32_t CSTmAuxTVOutput::SetControl(stm_output_control_t ctrl, uint32_t val)
{
  TRC( TRC_ID_MAIN_INFO, "-  ctrl = %d val = %u", ctrl, val );
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_SOURCE_SELECT:
    {
      /* Just to maintain the API */
      if(val != STM_VIDEO_SOURCE_AUX_COMPOSITOR)
        return STM_OUT_INVALID_VALUE;
      break;
    }
    case OUTPUT_CTRL_AUDIO_SOURCE_SELECT:
    {
      /* Just to maintain the API */
      if(val != STM_AUDIO_SOURCE_NONE)
        return STM_OUT_INVALID_VALUE;
      break;
    }
    case OUTPUT_CTRL_EXTERNAL_SYNC_SHIFT:
    {
	  /*
       * Will take effect on the next output restart
       */
      m_uExternalSyncShift = val;
      break;
    }
    case OUTPUT_CTRL_EXTERNAL_SYNC_INVERT:
    {
	  /*
       * Will take effect on the next output restart
       */
      m_bInvertExternalSyncs = (val!=0);
      break;
    }
    default:
      return CSTmMasterOutput::SetControl(ctrl,val);
  }

  return STM_OUT_OK;
}


uint32_t CSTmAuxTVOutput::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_EXTERNAL_SYNC_SHIFT:
      *val = m_uExternalSyncShift;
      break;
    case OUTPUT_CTRL_EXTERNAL_SYNC_INVERT:
      *val = m_bInvertExternalSyncs;
      break;
    case OUTPUT_CTRL_DAC_POWER_DOWN:
      *val = m_bDacPowerDown?1:0;
      break;
    default:
      return CSTmMasterOutput::GetControl(ctrl,val);
  }

  return STM_OUT_OK;
}


uint32_t CSTmAuxTVOutput::SetCompoundControl(stm_output_control_t ctrl, void *newVal)
{
  uint32_t ret = STM_OUT_NO_CTRL;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
    {
      /*
       * For SD Modes we're always trying to calibrate both DENC and HDF
       */
      if(m_pDENC && (m_pDENC->GetOwner() == this))
        ret = m_pDENC->SetCompoundControl(ctrl,newVal);

      if((ret == STM_OUT_OK) || (ret == STM_OUT_NO_CTRL))
      {
        if(m_pHDFormatter && (m_pHDFormatter->GetOwner() == this))
          ret = m_pHDFormatter->SetCompoundControl(ctrl,newVal);
      }
      break;
    }
    default:
      ret = CSTmMasterOutput::SetCompoundControl(ctrl,newVal);
      break;
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


uint32_t CSTmAuxTVOutput::GetCompoundControl(stm_output_control_t ctrl, void *val) const
{
  uint32_t ret = STM_OUT_NO_CTRL;

  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
    {
      /*
       * For SD Modes we're always trying to get both DENC and HDF calibration values
       */
      if(m_pDENC && (m_pDENC->GetOwner() == this))
          ret = m_pDENC->GetCompoundControl(ctrl,val);

      if((ret == STM_OUT_OK) || (ret == STM_OUT_NO_CTRL))
      {
        if(m_pHDFormatter && (m_pHDFormatter->GetOwner() == this))
          ret = m_pHDFormatter->GetCompoundControl(ctrl,val);
      }
      break;
    }
    default:
      ret = CSTmMasterOutput::GetCompoundControl(ctrl,val);
      break;
  }

  return ret;
}


bool CSTmAuxTVOutput::SetFilterCoefficients(const stm_display_filter_setup_t *f)
{
  bool ret = false;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(f->type)
  {
    case HDF_COEFF_2X_LUMA ... HDF_COEFF_4X_CHROMA:
    {
      ret = m_pHDFormatter->SetFilterCoefficients(f);
      break;
    }
    case DENC_COEFF_LUMA:
    case DENC_COEFF_CHROMA:
    {
      if(m_pDENC&&m_bUsingDENC)
      {
        ret = m_pDENC->SetFilterCoefficients(f);
      }
      break;
    }
    default:
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


void CSTmAuxTVOutput::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsSuspended)
    return;

  if ((m_pHDFormatter->GetOwner() == this)||(m_pHDFormatter->GetOwner() == 0))
  {
    m_pHDFormatter->Resume();
  }

  if(m_bIsStarted)
  {
    /*
     * We need to go and make sure all the clock setup is correct again
     */
    ConfigureDisplayClocks(&m_CurrentOutputMode);
    SetVTGSyncs(&m_CurrentOutputMode);
  }

  CSTmMasterOutput::Resume();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
