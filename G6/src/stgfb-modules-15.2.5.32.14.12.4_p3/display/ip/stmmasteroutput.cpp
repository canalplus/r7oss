/***********************************************************************
 *
 * File: display/ip/stmmasteroutput.cpp
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
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
#include <display/generic/DisplayPlane.h>
#include <display/generic/DisplayMixer.h>

#include <display/ip/stmdenc.h>
#include <display/ip/displaytiming/stmvtg.h>

#include "stmmasteroutput.h"


CSTmMasterOutput::CSTmMasterOutput(const char     *name,
                                   uint32_t        id,
                                   CDisplayDevice *pDev,
                                   CSTmDENC       *pDENC,
                                   CSTmVTG        *pVTG,
                                   CDisplayMixer  *pMixer) : COutput(name, id, pDev, (uint32_t)pVTG)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDevReg = (uint32_t*)pDev->GetCtrlRegisterBase();

  m_pDENC     = pDENC;
  m_pVTG      = pVTG;
  m_pMixer    = pMixer;

  m_ulCapabilities |= OUTPUT_CAPS_DISPLAY_TIMING_MASTER;
  m_ulCapabilities |= m_pMixer->GetCapabilities();

  if(m_pDENC)
    m_ulCapabilities |= m_pDENC->GetCapabilities();

  m_bUsingDENC  = false;

  /*
   * Analogue outputs don't have hot plug status, so we say they are always
   * connected.
   */
  m_displayStatus = STM_DISPLAY_CONNECTED;

  vibe_os_zero_memory(&m_PendingMode, sizeof(stm_display_mode_t));
  m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;

  m_ulBrightness = 0x80;
  m_ulSaturation = 0x80;
  m_ulContrast   = 0x80;
  m_ulHue        = 0x80;

  /*
   * Just pick some common default numbers to avoid divide by zero exceptions
   */
  m_maxDACVoltage  = 1400; // in mV
  m_DACSaturation  = 1023; // use the full 10bit DAC range
  RecalculateDACSetup();

  m_VTGErrorCount = 1;

  m_bPendingStart = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmMasterOutput::~CSTmMasterOutput() {}


bool CSTmMasterOutput::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Paranoia part 1. Lock out interrupt processing while we test and shutdown
   * the mixer, to prevent a plane's update processing enabling itself on the
   * mixer between the test and the Stop.
   */
  vibe_os_lock_resource(m_lock);
  {
    if(m_pMixer->HasEnabledPlanes())
    {
      TRC( TRC_ID_MAIN_INFO, "mixer has enabled planes" );
      m_pMixer->UpdateFromOutput(this, STM_PLANE_OUTPUT_STOPPED);
    }

    this->DisableDACs();

    m_pMixer->Stop();
  }
  vibe_os_unlock_resource(m_lock);

  if(m_bUsingDENC)
    m_pDENC->Stop();

  /*
   * Paranoia part 2. Force a vsync to be generated to ensure the mixer and
   * DENC double buffered registers have switched to the stopped values.
   */
  m_pVTG->ResetCounters();

  m_pVTG->Stop();

  vibe_os_lock_resource(m_lock);
  {
    m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
    m_bUsingDENC = false;

    COutput::Stop();
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmMasterOutput::TryModeChange(const stm_display_mode_t* pModeLine)
{
  if(!m_bIsStarted)
    return false;

  TRC( TRC_ID_MAIN_INFO, "(%p), current/new mode: %d/%d", this, m_CurrentOutputMode.mode_id, pModeLine->mode_id );

  if(pModeLine->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
  {
    if(!m_pDENC)
      return false;

    if(AreModesIdentical(m_CurrentOutputMode,*pModeLine))
    {
      if(m_CurrentOutputMode.mode_params.output_standards != pModeLine->mode_params.output_standards)
      {
        if(m_bUsingDENC)
        {
          TRC( TRC_ID_MAIN_INFO, "changing DENC standard" );
          m_pDENC->Start(this, pModeLine);
        }
      }

      /*
       * Always keep a note of any changed standard (or flags) in case the DENC
       * gets re-started later due to an output format re-configuration.
       */
      COutput::Start(pModeLine);

      return true;
    }
  }
  else
  {
    /*
     * This supports changing between a select number of very closely
     * related modes, which requires only the fsynth register to be
     * changed and possibly the HDMI video info frame to be updated.
     * However getting all the hardware updated at the beginning of the
     * same vsync and glitch free is a challenge, so we have to
     * synchronize things in the VTG interrupt handler.
     */
    TRC( TRC_ID_MAIN_INFO, "trying VTG update" );

    vibe_os_lock_resource(m_lock);

    /*
     * Don't go any further if we already have a mode update pending,
     * the caller will have to stop the output if they really want to
     * do the change, which will cancel the outstanding update.
     */
    if(m_PendingMode.mode_id != STM_TIMING_MODE_RESERVED)
    {
      TRC( TRC_ID_ERROR, "Failed, VTG update already pending m_PendingMode ID = %d m_CurrentMode ID = %d",m_PendingMode.mode_id,m_CurrentOutputMode.mode_id );
      vibe_os_unlock_resource(m_lock);
      return false;
    }

    m_PendingMode = *pModeLine;
    vibe_os_unlock_resource(m_lock);

    if(m_pVTG->RequestModeUpdate(pModeLine))
      return true;

    /*
     * Cancel the failed pending update
     */
    m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
  }

  TRC( TRC_ID_MAIN_INFO, "failed, new mode is not compatible" );

  return false;
}


const stm_display_mode_t* CSTmMasterOutput::SupportedMode(const stm_display_mode_t *mode) const
{
  return COutput::GetSlavedOutputsSupportedMode(mode);
}


OutputResults CSTmMasterOutput::Start(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  TRC( TRC_ID_MAIN_INFO, "m_pVTG = %p m_pDENC = %p m_pMixer = %p",m_pVTG,m_pDENC,m_pMixer );

  // Check this object has been sanely set up
  ASSERTF((m_pVTG && m_pMixer),
          ("CSTmMasterOutput::Start Error: Output not initialised correctly\n"));

  ASSERTF(pModeLine, ("CSTmMasterOutput::Start Error: NULL mode line\n"));

  if(m_bIsSuspended)
  {
    TRC( TRC_ID_ERROR, "output is suspended" );
    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return STM_OUT_BUSY;
  }

  /*
   * First try to change the display mode on the fly, if that works there is
   * nothing else to do.
   */
  if(TryModeChange(pModeLine))
  {
    TRC( TRC_ID_MAIN_INFO, "mode change successful" );

    /* update MISR configuration to match new mode */
    UpdateMisrCtrl();

    m_bPendingStart = false;
    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return STM_OUT_OK;
  }

  vibe_os_lock_resource(m_lock);

  if(!m_pMixer->Start(pModeLine))
  {
    TRC( TRC_ID_ERROR, "Mixer start failed" );
    goto stop_and_exit;
  }

  /*
   * Set all Slaved Outputs VTGSyncs Before Starting the VTG
   */
  if(!SetSlavedOutputsVTGSyncs(pModeLine))
  {
    TRC( TRC_ID_ERROR, "Can't Set VTG Syncs for slaved ouptuts" );
    goto stop_and_exit;
  }

  if(!m_pVTG->Start(pModeLine))
  {
    TRC( TRC_ID_ERROR, "VTG start failed" );
    goto stop_and_exit;
  }

  if(m_bUsingDENC)
  {
    /*
     * Only start the DENC for SD modes, which includes ED modes for
     * parts containing a PDENC (variant that supports progressive ED
     * modes)
     */
    if(!m_pDENC->Start(this, pModeLine))
    {
      TRC( TRC_ID_ERROR, "DENC start failed" );
      goto stop_and_exit;
    }

    /*
     * Now we have started the DENC, set this output's PSI and
     * signal range control setup.
     */
    ProgramPSIControls();
    m_pDENC->SetControl(OUTPUT_CTRL_CLIP_SIGNAL_RANGE, (uint32_t)m_signalRange);
  }

  COutput::Start(pModeLine);

  vibe_os_unlock_resource(m_lock);

  /*
   * Notify Mixer for this Output start :
   * m_CurrentOutputMode should be valid at this point
   * This should be done outside the locked resources state
   */
  m_pMixer->UpdateFromOutput(this, STM_PLANE_OUTPUT_STARTED);

  vibe_os_lock_resource(m_lock);
  /*
   * Make sure that the current output format and DAC configuration are
   * applied, now that the mode is active, then turn the DACs on.
   */
  if(!this->SetOutputFormat(m_ulOutputFormat))
  {
    TRC( TRC_ID_ERROR, "Requested output format cannot be configured" );
    goto stop_and_exit;
  }

  this->EnableDACs();
  m_bPendingStart = false;

  vibe_os_unlock_resource(m_lock);

  /* update MISR configuration to match new mode */
  UpdateMisrCtrl();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return STM_OUT_OK;

stop_and_exit:
  vibe_os_unlock_resource(m_lock);
  /*
   * Cleanup any partially successful hardware setup.
   */
  this->Stop();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return STM_OUT_INVALID_VALUE;
}


void CSTmMasterOutput::Suspend(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return;

  this->DisableDACs();

  if(m_pVTG)
    m_pVTG->Stop();

  m_pMixer->Suspend();

  COutput::Suspend();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmMasterOutput::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsSuspended)
    return;

  m_pMixer->Resume();

  if(m_bIsStarted)
  {
    if(m_pVTG)
      m_pVTG->Start(GetCurrentDisplayMode());

    /*
     * Restart the DENC to reprogram its configuration in case the hardware was
     * in reset during the suspend.
     */
    if(m_bUsingDENC)
      m_pDENC->Start(this, GetCurrentDisplayMode());

    /*
     * Restore the output format configuration in case the hardware was in reset
     * during the suspend.
     */
    this->SetOutputFormat(m_ulOutputFormat);
    /*
     * Enable the physical outputs.
     */
    this->EnableDACs();
  }

  COutput::Resume();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmMasterOutput::UpdateOutputMode(const stm_display_mode_t &mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if(m_bIsStarted)
    m_CurrentOutputMode = mode;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmMasterOutput::HandleInterrupts(void)
{
  stm_time64_t now         = vibe_os_get_system_time();
  uint32_t     timingevent = m_pVTG->GetInterruptStatus();

  if(timingevent == STM_TIMING_EVENT_NONE)
  {
    m_LastVTGEvent = STM_TIMING_EVENT_NONE;
    return true;
  }

  vibe_os_lock_resource(m_lock);

  if(!m_bIsStarted)
  {
    /*
     * Ignore timing events that arrive while we are still in the process of
     * starting the output.
     */
    vibe_os_unlock_resource(m_lock);
    return true;
  }

  if (timingevent != STM_TIMING_EVENT_LINE)
  {
    /*
     * This isn't a line only event, so we may have some work to do.
     *
     * TODO: update for clock doubled 3D modes.
     *
     * First check if we have had an on the fly mode change complete on this
     * vsync. If so then reset the VSync timing information (we may have changed
     * refresh frequency).
     */
    if((m_PendingMode.mode_id != STM_TIMING_MODE_RESERVED) &&
        AreModesIdentical(m_PendingMode, m_pVTG->GetCurrentMode()))
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmMasterOutput::HandleVSYNCInterrupt changing display mode m_PendingMode ID = %d", m_PendingMode.mode_id );
      COutput::Start(&m_PendingMode);
      m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
    }
/*
 * System time is not accurate in case of virtual platform
 * So no need to do this
 */
#if !defined(CONFIG_STM_VIRTUAL_PLATFORM)
    if(m_LastVTGEventTime != 0)
    {
      stm_time64_t timediff = now - m_LastVTGEventTime;

      if(timediff < 0LL)
      {
        TRC( TRC_ID_ERROR, "HandleVSYNCInterrupt(output=%u): - backwards time detected, last time = %lld now = %lld",GetID(),m_LastVTGEventTime,now );
      }
      else if(timediff >= (m_fieldframeDuration*2LL))
      {
        if(UNLIKELY (m_VTGErrorCount == 0))
        {
          TRC( TRC_ID_ERROR, "HandleVSYNCInterrupt(output=%u: - time discontinuity detected, vsync interval = %lld field duration = %lld",GetID(),timediff,m_fieldframeDuration );
          m_VTGErrorCount = 60;
        }
        else
        {
          m_VTGErrorCount--;
        }
      }
    }
#endif
  }

  m_LastVTGEvent     = timingevent;
  m_LastVTGEventTime = now;

  vibe_os_unlock_resource(m_lock);

  return true;
}


void CSTmMasterOutput::SoftReset(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsStarted)
  {
    m_pVTG->ResetCounters();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CSTmMasterOutput::UpdateHW(void)
{
  if(m_pDENC && m_bUsingDENC)
    m_pDENC->UpdateHW(m_LastVTGEvent);

  SetMisrData(m_LastVTGEventTime, m_LastVTGEvent);
}

void CSTmMasterOutput::SetMisrData(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt){}

void CSTmMasterOutput::UpdateMisrCtrl(void){}

bool CSTmMasterOutput::CanShowPlane(const CDisplayPlane *plane)
{
  if(plane->GetParentDevice()->GetID() != m_pDisplayDevice->GetID())
  {
    TRC( TRC_ID_ERROR, "Plane and output do not belong to the same device" );
    return false;
  }

  /* FIXME: Actually when loading the Media-Controller module, this last is
   * doing Connect/disconnect each available plane to each available Output.
   * So Checking for supported modes by CGMS standard will make connection
   * impossible for all the life of the box even if we do not install a
   * supported mode. As result we may replace this by a test on the Output
   * format to allow VBIPlane only for YUV Output
   */
  if(plane->isVbiPlane())
  {
    if((m_ulOutputFormat & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV)) == 0)
    {
      TRC( TRC_ID_MAIN_INFO, "VBI Plane is not supported by this output (format=0x%x)", m_ulOutputFormat );
      return false;
    }
  }

  return m_pMixer->PlaneValid(plane);
}


bool CSTmMasterOutput::ShowPlane(const CDisplayPlane *plane)
{
  TRC( TRC_ID_MAIN_INFO, "\"%s\" ID:%u", plane->GetName(), plane->GetID() );
  if(plane->GetParentDevice()->GetID() != m_pDisplayDevice->GetID())
  {
    TRC( TRC_ID_ERROR, "Plane and output do not belong to the same device" );
    return false;
  }

  return m_pMixer->EnablePlane(plane);
}


void CSTmMasterOutput::HidePlane(const CDisplayPlane *plane)
{
  TRC( TRC_ID_MAIN_INFO, "\"%s\" ID:%u", plane->GetName(), plane->GetID() );
  if(plane->GetParentDevice()->GetID() != m_pDisplayDevice->GetID())
  {
    TRC( TRC_ID_ERROR, "Plane and output do not belong to the same device" );
    return;
  }

  m_pMixer->DisablePlane(plane);
}


bool CSTmMasterOutput::SetPlaneDepth(const CDisplayPlane *plane, int depth, bool activate)
{
  TRC( TRC_ID_MAIN_INFO, "%u depth = %d",plane->GetID(),depth );

  if(plane->GetParentDevice()->GetID() != m_pDisplayDevice->GetID())
  {
    TRC( TRC_ID_ERROR, "Plane and output do not belong to the same device" );
    return false;
  }

  return m_pMixer->SetPlaneDepth(plane, depth, activate);
}


bool CSTmMasterOutput::GetPlaneDepth(const CDisplayPlane *plane, int *depth) const
{
  TRC( TRC_ID_MAIN_INFO, "%u",plane->GetID() );

  if(plane->GetParentDevice()->GetID() != m_pDisplayDevice->GetID())
  {
    TRC( TRC_ID_ERROR, "Plane and output do not belong to the same device" );
    return false;
  }

  return m_pMixer->GetPlaneDepth(plane, depth);
}


uint32_t CSTmMasterOutput::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_YCBCR_COLORSPACE:
    case OUTPUT_CTRL_BACKGROUND_ARGB:
    case OUTPUT_CTRL_BACKGROUND_VISIBILITY:
      return m_pMixer->SetControl(ctrl, newVal);

    case OUTPUT_CTRL_VIDEO_OUT_SELECT:
    {
      if(this->SetOutputFormat(newVal))
      {
        m_ulOutputFormat = newVal;
        if(m_bIsStarted)
          this->EnableDACs();
      }
      else
        return STM_OUT_INVALID_VALUE;

      break;
    }
    case OUTPUT_CTRL_CLOCK_ADJUSTMENT:
    {
      /*
       * The value passed in is actually signed 2's complement
       */
      if (m_ulOutputFormat&(STM_VIDEO_OUT_CVBS | STM_VIDEO_OUT_YC))
      {
        if ((int)newVal < -20 && (int)newVal >= -500)
        {
          TRC( TRC_ID_ERROR, "Clamping adjustement to -20 to have correct subcarrier frequency" );
          newVal = -20;
        }
        if ((int)newVal > 20  && (int)newVal <= 500)
        {
          TRC( TRC_ID_ERROR, "Clamping adjustement to 20 to have correct subcarrier frequency" );
          newVal = 20;
        }
      }
      if(!m_pVTG->SetAdjustment((int)newVal))
        return STM_OUT_NO_CTRL;
      break;
    }
    case OUTPUT_CTRL_BRIGHTNESS:
    {
      m_ulBrightness = newVal;
      if(m_bIsStarted)
        ProgramPSIControls();

      break;
    }
    case OUTPUT_CTRL_SATURATION:
    {
      m_ulSaturation = newVal;
      if(m_bIsStarted)
        ProgramPSIControls();

      break;
    }
    case OUTPUT_CTRL_CONTRAST:
    {
      m_ulContrast = newVal;
      if(m_bIsStarted)
        ProgramPSIControls();

      break;
    }
    case OUTPUT_CTRL_HUE:
    {
      m_ulHue = newVal;
      if(m_bIsStarted)
        ProgramPSIControls();

      break;
    }
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      if(newVal> STM_SIGNAL_VIDEO_RANGE)
        return STM_OUT_INVALID_VALUE;

      m_signalRange = (stm_display_signal_range_t)newVal;
      if(m_bIsStarted && m_pDENC && m_bUsingDENC)
        m_pDENC->SetControl(OUTPUT_CTRL_CLIP_SIGNAL_RANGE, (uint32_t)m_signalRange);

      break;
    }
    case OUTPUT_CTRL_DAC123_MAX_VOLTAGE:
    {
      TRC( TRC_ID_MAIN_INFO, "max dac voltage = %u",newVal );
      m_maxDACVoltage = newVal;

      RecalculateDACSetup();
      if(m_pDENC && (m_pDENC->GetOwner()==this)) m_pDENC->SetControl(ctrl,newVal);

      this->SetOutputFormat(m_ulOutputFormat);

      break;
    }
    case OUTPUT_CTRL_MAX_PIXEL_CLOCK:
    {
      if(newVal < 300000000)
        m_ulMaxPixClock = newVal;
      else
        return STM_OUT_INVALID_VALUE;

      break;
    }
    default:
        return (m_pDENC && (m_pDENC->GetOwner()==this))?m_pDENC->SetControl(ctrl, newVal):STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CSTmMasterOutput::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_YCBCR_COLORSPACE:
    case OUTPUT_CTRL_BACKGROUND_ARGB:
    case OUTPUT_CTRL_BACKGROUND_VISIBILITY:
      return m_pMixer->GetControl(ctrl,val);

    case OUTPUT_CTRL_CLOCK_ADJUSTMENT:
    {
      *val = (uint32_t)m_pVTG->GetAdjustment(); /* Note this is actually signed */
      break;
    }
    case OUTPUT_CTRL_VIDEO_SOURCE_SELECT:
      *val = m_VideoSource;
      break;
    case OUTPUT_CTRL_AUDIO_SOURCE_SELECT:
      *val = m_AudioSource;
      break;
    case OUTPUT_CTRL_VIDEO_OUT_SELECT:
      *val = m_ulOutputFormat;
      break;
    case OUTPUT_CTRL_BRIGHTNESS:
      *val = m_ulBrightness;
      break;
    case OUTPUT_CTRL_SATURATION:
      *val = m_ulSaturation;
      break;
    case OUTPUT_CTRL_CONTRAST:
      *val = m_ulContrast;
      break;
    case OUTPUT_CTRL_HUE:
      *val = m_ulHue;
      break;
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
      *val = m_signalRange;
      break;
    case OUTPUT_CTRL_DAC123_MAX_VOLTAGE:
      *val = m_maxDACVoltage;
      break;
    case OUTPUT_CTRL_MAX_PIXEL_CLOCK:
      *val = m_ulMaxPixClock;
      break;
    default:
      if(!m_pDENC || (m_pDENC->GetOwner()!=this))
      {
        *val = 0;
        return STM_OUT_NO_CTRL;
      }
      return m_pDENC->GetControl(ctrl,val);
  }

  return STM_OUT_OK;
}


uint32_t CSTmMasterOutput::SetCompoundControl(stm_output_control_t ctrl, void *newVal)
{
  uint32_t ret = STM_OUT_NO_CTRL;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(ctrl)
  {
    case OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS:
    {
      const stm_display_filter_setup_t *f = (const stm_display_filter_setup_t *)newVal;
      ret = this->SetFilterCoefficients(f)?STM_OUT_OK:STM_OUT_INVALID_VALUE;
      break;
    }
    case OUTPUT_CTRL_VPS:
    {
      if(m_pDENC)
        ret = m_pDENC->SetVPS((stm_display_vps_t*)newVal)?STM_OUT_OK:STM_OUT_INVALID_VALUE;
      break;
    }
    default:
      ret = COutput::SetCompoundControl(ctrl, newVal);
  }

  /*
   * Following Output controls should be transmitted also to connected planes
   */
  switch(ctrl)
  {
    case OUTPUT_CTRL_DISPLAY_ASPECT_RATIO:
    {
      if(ret == STM_OUT_OK)
        m_pMixer->UpdateFromOutput(this, STM_PLANE_OUTPUT_UPDATED);
      break;
    }
    default:
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


bool CSTmMasterOutput::SetFilterCoefficients(const stm_display_filter_setup_t *f)
{
  bool ret = false;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(f->type)
  {
    case DENC_COEFF_LUMA:
    case DENC_COEFF_CHROMA:
      if(m_pDENC)
        ret = m_pDENC->SetFilterCoefficients(f);
      break;
    default:
      break;
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


void CSTmMasterOutput::ProgramPSIControls(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  /*
   * Default is to defer this down to the DENC, future pipelines may
   * support control on HD DACs as well.
   */
  if(m_pDENC && m_bUsingDENC)
  {
    m_pDENC->SetControl(OUTPUT_CTRL_BRIGHTNESS,m_ulBrightness);
    m_pDENC->SetControl(OUTPUT_CTRL_CONTRAST,  m_ulContrast);
    m_pDENC->SetControl(OUTPUT_CTRL_SATURATION,m_ulSaturation);
    m_pDENC->SetControl(OUTPUT_CTRL_HUE,       m_ulHue);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CSTmMasterOutput::RecalculateDACSetup(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Calculate DAC levels for synchronisation insertion and rescaling.
   * We need to set things up to give the correct output voltages
   * based on the maximum voltage range of the DACs. This is partly
   * dependent on the reference voltage setup on a specific board, so this
   * can be set by the OUTPUT_CTRL_DAC_MAX_VOLTAGE and OUTPUT_CTRL_DAC_SATURATION
   * controls. The voltage is specified in mV.
   */

  /*
   * RGB and Y active signals are offset by 0.321V to allow
   * for negative sync pulses.
   */
  m_DAC_321mV = DIV_ROUNDED((m_DACSaturation*321),m_maxDACVoltage);

  TRC( TRC_ID_MAIN_INFO, "321mv = 0x%x (%u)",m_DAC_321mV,m_DAC_321mV );

  /*
   * Sync pulses are +-43IRE from the blanking level, where
   * 43IRE = 0.301V
   */
  m_DAC_43IRE = DIV_ROUNDED((m_DACSaturation*301),m_maxDACVoltage);

  TRC( TRC_ID_MAIN_INFO, "43IRE = 0x%x (%u)",m_DAC_43IRE,m_DAC_43IRE );

  /*
   * All video signals (including Pr & Pb) have a 0.7V range
   */
  m_DAC_700mV = DIV_ROUNDED((m_DACSaturation*700),m_maxDACVoltage);

  TRC( TRC_ID_MAIN_INFO, "700mV = 0x%x (%u)",m_DAC_700mV,m_DAC_700mV );

  /*
   * We assume that for RGB we map the full range of output from
   * the compositor (0-1023) to the DAC_Video_Range
   */
  m_DAC_RGB_Scale = DIV_ROUNDED(m_DAC_700mV,100);


#define CONVERT_TO_10BIT(x) (((x)<<2) | ((x)>>6))
  const uint32_t Compositor_Y_White_Value = CONVERT_TO_10BIT(235);
  const uint32_t Compositor_Y_Black_Value = CONVERT_TO_10BIT(16);
  const uint32_t Compositor_C_Max_Value   = CONVERT_TO_10BIT(240);
  const uint32_t Compositor_C_Mid_Value   = CONVERT_TO_10BIT(128);
  const uint32_t Compositor_C_Min_Value   = CONVERT_TO_10BIT(16);

  TRC( TRC_ID_MAIN_INFO, "Yw = 0x%x (%u)",Compositor_Y_White_Value,Compositor_Y_White_Value );
  TRC( TRC_ID_MAIN_INFO, "Yb = 0x%x (%u)",Compositor_Y_Black_Value,Compositor_Y_Black_Value );
  TRC( TRC_ID_MAIN_INFO, "Cmax = 0x%x (%u)",Compositor_C_Max_Value,Compositor_C_Max_Value );
  TRC( TRC_ID_MAIN_INFO, "Cmin = 0x%x (%u)",Compositor_C_Min_Value,Compositor_C_Min_Value );

  /*
   * Chroma is centered around 0.653V
   */
  const uint32_t DAC_653mV = DIV_ROUNDED((m_DACSaturation*653),m_maxDACVoltage);
  TRC( TRC_ID_MAIN_INFO, "653mv = 0x%x (%u)",DAC_653mV,DAC_653mV );

  /*
   * We scale the _active_ range of luma and chroma from the compositor to
   * the DAC_Video_Range. Then we offset the output so that the Black/0 level
   * chroma levels are located at the appropriate voltages.
   */
  const uint32_t scale_100_percent = 1024; // Register value for 100% scale

  m_DAC_Y_Scale   = DIV_ROUNDED((scale_100_percent * m_DAC_700mV), (Compositor_Y_White_Value - Compositor_Y_Black_Value));
  m_DAC_Y_Offset  = m_DAC_321mV - DIV_ROUNDED((Compositor_Y_Black_Value * m_DAC_Y_Scale) , scale_100_percent);

  m_DAC_C_Scale   = DIV_ROUNDED_UP((scale_100_percent * m_DAC_700mV), (Compositor_C_Max_Value - Compositor_C_Min_Value));
  m_DAC_C_Offset  = DAC_653mV - DIV_ROUNDED((Compositor_C_Mid_Value * m_DAC_C_Scale), scale_100_percent);

  TRC( TRC_ID_MAIN_INFO, "Yscale = 0x%x (%u)",m_DAC_Y_Scale,m_DAC_Y_Scale );
  TRC( TRC_ID_MAIN_INFO, "Yoff   = 0x%x (%u)",m_DAC_Y_Offset,m_DAC_Y_Offset );
  TRC( TRC_ID_MAIN_INFO, "Cscale = 0x%x (%u)",m_DAC_C_Scale,m_DAC_C_Scale );
  TRC( TRC_ID_MAIN_INFO, "Coff   = 0x%x (%u)",m_DAC_C_Offset,m_DAC_C_Offset );

  /*
   * Used in the AWG based devices to shift its signal output on the chroma
   * channels. Although we don't support syncs on chroma at the moment, this is
   * still needed to get the correct chroma blanking level, where the syncs
   * would otherwise be.
   */
  m_AWG_Y_C_Offset = DAC_653mV - m_DAC_Y_Offset;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmMasterOutput::SetClockReference(stm_clock_ref_frequency_t refClock, int error_ppm)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pVTG->SetClockReference(refClock,error_ppm);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


stm_display_metadata_result_t CSTmMasterOutput::QueueMetadata(stm_display_metadata_t *m)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(m->type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
    case STM_METADATA_TYPE_TELETEXT:
    case STM_METADATA_TYPE_CLOSED_CAPTION:
      if(m_pDENC && m_bUsingDENC)
        return m_pDENC->QueueMetadata(m);
      else
        return STM_METADATA_RES_QUEUE_UNAVAILABLE;

    default:
      return STM_METADATA_RES_UNSUPPORTED_TYPE;
  }
}


void CSTmMasterOutput::FlushMetadata(stm_display_metadata_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
    case STM_METADATA_TYPE_TELETEXT:
    case STM_METADATA_TYPE_CLOSED_CAPTION:
      if(m_pDENC && m_bUsingDENC)
        m_pDENC->FlushMetadata(type);

      break;
    default:
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
