/***********************************************************************
 *
 * File: display/ip/displaytiming/stmvtg.cpp
 * Copyright (c) 2000-2011 STMicroelectronics Limited.
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
#include <display/generic/Output.h>

#include "stmvtg.h"

CSTmVTG::CSTmVTG(CDisplayDevice* pDev, CSTmFSynth *pFSynth, bool bDoubleClocked, unsigned nbr_sync_outputs)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmVTG::CSTmVTG pDev = %p pFSynth = %p", pDev, pFSynth );

  m_pFSynth      = pFSynth;

  vibe_os_zero_memory(&m_CurrentMode, sizeof(stm_display_mode_t));
  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;

  vibe_os_zero_memory(&m_PendingMode, sizeof(stm_display_mode_t));
  m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;

  m_pSlavedVTG   = 0;

  m_pendingSync = 0;

  m_bDoubleClocked = bDoubleClocked;

  m_bDisabled    = true; // Initial state is that syncs are disabled
  m_bSuppressMissedInterruptMessage = false;

  m_lock         = vibe_os_create_resource_lock();

  m_bUpdateOnlyClockFrequency = false;

  m_max_sync_outputs = nbr_sync_outputs;

  m_syncParams = new stm_vtg_sync_params_t [m_max_sync_outputs+1];

  for(int outputid=0; outputid<=m_max_sync_outputs; outputid++)
  {
    m_syncParams[outputid].m_HSyncOffsets  = 0;
    m_syncParams[outputid].m_VSyncHOffsets = 0;
    m_syncParams[outputid].m_syncType      = STVTG_SYNC_TIMING_MODE;
  }

  /*
   * Reset Output Window Rectangle
   */
   m_UpdateOutputRect = false;
   vibe_os_zero_memory(&m_OutputWindowRect,sizeof(stm_rect_t));

  m_bIsSlaveVTG  = false;
  m_SlavedVTGInputSyncType = STVTG_SYNC_POSITIVE;
  m_SlavedVTGVSyncHDelay = 0;
  m_SlavedVTGHSyncDelay = 0;
  m_SlavedVTGExternalSyncSource = 0;
}


CSTmVTG::~CSTmVTG()
{
  delete [] m_syncParams;
  vibe_os_delete_resource_lock(m_lock);
}


bool CSTmVTG::RegisterSlavedVTG(CSTmVTG            *pSlavedVTG,
                                stm_vtg_sync_type_t SyncType,
                                int                 ExternalSyncSourceID,
                                int                 VSyncHDelay,
                                int                 HSyncDelay)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
    return false;

  m_pSlavedVTG                  = pSlavedVTG;
  m_SlavedVTGVSyncHDelay        = VSyncHDelay;
  m_SlavedVTGHSyncDelay         = HSyncDelay;
  m_SlavedVTGExternalSyncSource = ExternalSyncSourceID;

  m_pSlavedVTG->SetIsSlave(true);
  m_pSlavedVTG->SetSlavedVTGInputSyncType(SyncType);

  /*
   * Change all of the slaved sync output offsets to the default delays
   * imposed by chaining of the VTGs.
   */
  for(int i=1; i <= m_pSlavedVTG->GetNumSyncOutputs(); i++)
  {
    m_pSlavedVTG->SetHSyncOffset(i, HSyncDelay);
    m_pSlavedVTG->SetVSyncHOffset(i, VSyncHDelay);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmVTG::UnRegisterSlavedVTG(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
  {
    m_pSlavedVTG->SetIsSlave(false);
    m_pSlavedVTG->DisableSyncs();
    m_pSlavedVTG = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmVTG::SetSlavedVTGInputSyncType(stm_vtg_sync_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
    m_pSlavedVTG->SetSlavedVTGInputSyncType(type);
  else
    m_SlavedVTGInputSyncType = type;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmVTG::Start(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSlaveVTG)
  {
    TRC( TRC_ID_ERROR, "Cannot start slaved VTG as Master" );
    return false;
  }

  if(!m_pFSynth->Start(pModeLine->mode_timing.pixel_clock_freq))
  {
    TRC( TRC_ID_ERROR, "Unable to start FSynth at required frequency" );
    return false;
  }

  if(m_pSlavedVTG)
  {
    if(!m_pSlavedVTG->Start(pModeLine, m_SlavedVTGExternalSyncSource))
    {
      TRC( TRC_ID_ERROR, "Failed to start slaved VTG in required mode" );
      return false;
    }
  }

  /*
   * NOTE: This does _not_ set m_CurrentMode, a subclass will do this when
   *       the mode is really active (the programming may be deferred in some
   *       cases).
   */
  return true;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmVTG::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_lock_resource(m_lock);

  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
  m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
  m_bUpdateOnlyClockFrequency = false;

  vibe_os_unlock_resource(m_lock);

  if(m_pSlavedVTG)
    m_pSlavedVTG->Stop();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

uint32_t CSTmVTG::ReadHWInterruptStatus(void)
{
  ASSERTF(false,("The subclass should never call this is it hasn't been overridden by that subclass\n"));
  return 0; // Avoid compiler warning
}

void CSTmVTG::ProgramVTGTimings(const stm_display_mode_t*)
{
  ASSERTF(false,("Probably no implementation of ProgramVTGTimings on slaved VTG\n"));
}


void CSTmVTG::ProgramOutputWindow(const stm_display_mode_t* pModeLine)
{
  ASSERTF(false,("Probably no implementation of ProgramOutputWindow on slaved VTG\n"));
}


bool CSTmVTG::RequestModeUpdate(const stm_display_mode_t *pModeLine)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmVTG::RequestModeUpdate in" );

  vibe_os_lock_resource(m_lock);

  if(!IsStarted())
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmVTG::RequestModeUpdate failed, VTG is stopped" );
    vibe_os_unlock_resource(m_lock);
    return false;
  }

  if(AreModeTimingsIdentical(*pModeLine, m_CurrentMode))
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmVTG::RequestModeUpdate timings already active" );
    /*
     * Just update the current mode record with any flags changes.
     */
    m_CurrentMode = *pModeLine;
    vibe_os_unlock_resource(m_lock);
    return true;
  }

  if(AreModeTimingsCompatible(*pModeLine, m_CurrentMode)
     /* this is here to disallow fast mode switching between
        1080p29/1080p30 <-> 1080p59/1080p60 and 1080p25 <-> 1080p50,
        which will not work, because CSTmFSynth::Start(), as called from
        GetInterruptStatus() will be unable to solve the FSynth
        equation, as the FSynth divider will not be updated in this case. */
     && (  (m_CurrentMode.mode_params.vertical_refresh_rate >= 50000 && pModeLine->mode_params.vertical_refresh_rate >= 50000)
        || (m_CurrentMode.mode_params.vertical_refresh_rate < 50000  && pModeLine->mode_params.vertical_refresh_rate < 50000))
     )
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmVTG::RequestModeUpdate updating FSynth" );

    m_PendingMode = *pModeLine;
    m_bUpdateOnlyClockFrequency = true;

    vibe_os_unlock_resource(m_lock);
    TRC( TRC_ID_MAIN_INFO, "CSTmVTG::RequestModeUpdate out" );
    return true;
  }

  TRC( TRC_ID_MAIN_INFO, "CSTmVTG::RequestModeUpdate mode not compatible" );

  vibe_os_unlock_resource(m_lock);
  return false;
}


void CSTmVTG::GetHSyncPositions(int                        outputid,
                                const stm_display_mode_timing_t &mode_timing,
                                uint32_t                  *HStart,
                                uint32_t                  *HStop)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  long clocksperline;

  if(m_bDoubleClocked)
    clocksperline = mode_timing.pixels_per_line*2;
  else
    clocksperline = mode_timing.pixels_per_line;

  long start;
  long stop;

  if ((m_syncParams[outputid].m_syncType == STVTG_SYNC_TIMING_MODE && !mode_timing.hsync_polarity) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_INVERSE_TIMING_MODE && mode_timing.hsync_polarity))
  { // HSync is negative
    TRC( TRC_ID_MAIN_INFO, "CSTmVTG::GetHSyncPositions: syncid = %d negative", outputid );
    start = mode_timing.hsync_width;
    if(m_bDoubleClocked)
      start *= 2;

    stop = 0;
  }
  else
  { // HSync is positive
    TRC( TRC_ID_MAIN_INFO, "CSTmVTG::GetHSyncPositions: syncid = %d positive", outputid );
    start = 0;
    stop  = mode_timing.hsync_width;
    if(m_bDoubleClocked)
      stop *= 2;
  }

  long offset = m_syncParams[outputid].m_HSyncOffsets;

  if(m_bDoubleClocked)
    offset *= 2;

  /*
   * Limit the offset to half a line; in reality the offsets are only going to
   * be a handful of clocks so this is being a bit paranoid.
   */
  if(offset >= (int32_t)(clocksperline/2))
    offset = (int32_t)(clocksperline/2) - 1L;

  if(offset <= -((int32_t)(clocksperline/2)))
    offset = -((int32_t)(clocksperline/2)) + 1L;

  start += offset;
  stop  += offset;

  if(start<0)
    start += clocksperline;

  if(stop<0)
    stop += clocksperline;

  if(start >= clocksperline)
    start -= clocksperline;

  if(stop >= clocksperline)
    stop -= clocksperline;

  TRC( TRC_ID_MAIN_INFO, "CSTmVTG::GetHSyncPositions: syncid = %d start = %lu stop = %lu", outputid,start,stop );

  *HStart = start;
  *HStop  = stop;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

/*
 * Set an offset from href for the hsync signals to be generated, this can
 * be negative. Note that outputid 0 is deliberately ignored.
 */
void CSTmVTG::SetHSyncOffset(int outputid, int offset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(outputid>0 && outputid<=m_max_sync_outputs)
  {
    m_syncParams[outputid].m_HSyncOffsets = offset;
    m_pendingSync = (1LU<<(outputid-1));
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
 }

/*
 * Set an offset in pixels from vref for the vsync signals to be generated,
 * this can be negative. Note that outputid 0 is deliberately ignored.
 */
void CSTmVTG::SetVSyncHOffset(int outputid, int offset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(outputid>0 && outputid<=m_max_sync_outputs)
  {
    m_syncParams[outputid].m_VSyncHOffsets = offset;
    m_pendingSync |= (1LU<<(outputid-1));
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmVTG::SetSlaveHSyncOffset(int outputid, int offset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
    m_pSlavedVTG->SetHSyncOffset(outputid, (offset + m_SlavedVTGHSyncDelay));

   TRCOUT( TRC_ID_MAIN_INFO, "" );
 }


void CSTmVTG::SetSlaveVSyncHOffset(int outputid, int offset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
    m_pSlavedVTG->SetVSyncHOffset(outputid, (offset + m_SlavedVTGVSyncHDelay));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


/*
 * Set the type of a particular sync output, which can include the
 * polarity of the ref signals.
 */
void CSTmVTG::SetSyncType(int outputid, stm_vtg_sync_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(outputid>=0 && outputid<=m_max_sync_outputs)
  {
    m_syncParams[outputid].m_syncType = type;
    if(outputid>0)
      m_pendingSync |= (1LU<<(outputid-1));
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmVTG::SetSlaveSyncType(int outputid, stm_vtg_sync_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
    m_pSlavedVTG->SetSyncType(outputid, type);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmVTG::SetSlaveBnottopType(stm_display_sync_id_t sync, stm_vtg_sync_Bnottop_type_t type)
{
  bool RetOk = false;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pSlavedVTG)
    RetOk = m_pSlavedVTG->SetBnottopType(sync, type);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return RetOk;
}


bool CSTmVTG::SetOutputWindowRectangle(const uint16_t XStart, const uint16_t YStart, const uint32_t Width, const uint32_t Height)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  stm_display_mode_t Mode;

  Mode.mode_id = STM_TIMING_MODE_RESERVED;

  if(IsPending())
    Mode = m_PendingMode;
  else if(IsStarted())
    Mode = m_CurrentMode;

  if(Mode.mode_id == STM_TIMING_MODE_RESERVED)
  {
    TRC( TRC_ID_ERROR, "Unknown VTG Status !!" );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "In: XStart=%d, YStart=%d, Width=%d, Height=%d", XStart, YStart, Width, Height );

  if(  ((XStart > 0) && (XStart <= Mode.mode_params.active_area_start_pixel))
    && ((YStart >  0) && (YStart <= Mode.mode_params.active_area_start_line))
    && ((Width == 0)  || ((XStart + Width) <= Mode.mode_timing.pixels_per_line))
    && ((Height == 0) || ((YStart + Height) <= Mode.mode_timing.lines_per_frame)))
  {
    vibe_os_lock_resource(m_lock);

    m_UpdateOutputRect   = false;
    m_OutputWindowRect.x = XStart;
    m_OutputWindowRect.y = YStart;

    if(Width == 0)
      m_OutputWindowRect.width  = Mode.mode_timing.pixels_per_line - XStart;
    else
      m_OutputWindowRect.width  = Width;

    if(Height == 0)
      m_OutputWindowRect.height = Mode.mode_timing.lines_per_frame - YStart;
    else
      m_OutputWindowRect.height = Height;

    m_UpdateOutputRect = true;
    vibe_os_unlock_resource(m_lock);
  }

  TRC( TRC_ID_MAIN_INFO, "Out: XStart=%d, YStart=%d, Width=%d, Height=%d", m_OutputWindowRect.x, m_OutputWindowRect.y, m_OutputWindowRect.width, m_OutputWindowRect.height );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return (m_UpdateOutputRect);
}
