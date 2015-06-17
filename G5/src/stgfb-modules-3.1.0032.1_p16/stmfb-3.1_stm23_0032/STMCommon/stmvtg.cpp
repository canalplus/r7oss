/***********************************************************************
 *
 * File: STMCommon/stmvtg.cpp
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>

#include "stmvtg.h"
#include "stmfsynth.h"

CSTmVTG::CSTmVTG(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth, bool bDoubleClocked)
{
  DEBUGF2(2,("CSTmVTG::CSTmVTG pDev = %px\n",pDev));

  m_pDevRegs     = (ULONG*)pDev->GetCtrlRegisterBase();
  m_ulVTGOffset  = ulRegOffset;

  m_pFSynth      = pFSynth;

  m_pCurrentMode = 0;
  m_pPendingMode = 0;

  m_bDoubleClocked = bDoubleClocked;

  m_bDisabled    = false;
  m_bSuppressMissedInterruptMessage = false;

  m_lock         = g_pIOS->CreateResourceLock();

  m_bUpdateClockFrequency = false;

  g_pIOS->ZeroMemory(&m_HSyncOffsets,sizeof(m_HSyncOffsets));
  g_pIOS->ZeroMemory(&m_VSyncHOffsets,sizeof(m_VSyncHOffsets));

  m_syncType[0] = STVTG_SYNC_TIMING_MODE;
  m_syncType[1] = STVTG_SYNC_TIMING_MODE;
  m_syncType[2] = STVTG_SYNC_TIMING_MODE;
  m_syncType[3] = STVTG_SYNC_TIMING_MODE;
}

CSTmVTG::~CSTmVTG()
{
  g_pIOS->DeleteResourceLock(m_lock);
}

bool CSTmVTG::RequestModeUpdate(const stm_mode_line_t *pModeLine)
{
  DEBUGF2(2,("CSTmVTG::RequestModeUpdate in\n"));

  if(!m_pCurrentMode)
  {
    DEBUGF2(2,("CSTmVTG::RequestModeUpdate failed, VTG is stopped\n"));
    return false;
  }

  if(pModeLine == m_pCurrentMode)
  {
    DEBUGF2(2,("CSTmVTG::RequestModeUpdate mode already active\n"));
    return true;
  }

  if(m_pCurrentMode->ModeParams.ScanType         == pModeLine->ModeParams.ScanType         &&
     m_pCurrentMode->ModeParams.ActiveAreaXStart == pModeLine->ModeParams.ActiveAreaXStart &&
     m_pCurrentMode->ModeParams.ActiveAreaWidth  == pModeLine->ModeParams.ActiveAreaWidth  &&
     m_pCurrentMode->ModeParams.FullVBIHeight    == pModeLine->ModeParams.FullVBIHeight    &&
     m_pCurrentMode->ModeParams.ActiveAreaHeight == pModeLine->ModeParams.ActiveAreaHeight &&
     m_pCurrentMode->TimingParams.PixelsPerLine == pModeLine->TimingParams.PixelsPerLine
     /* this is here to disallow fast mode switching between
        1080p29/1080p30 <-> 1080p59/1080p60 and 1080p25 <-> 1080p50,
        which will not work, because CSTmFSynth::Start(), as called from
        CSTmSDVTG::GetInterruptStatus() will be unable to solve the FSynth
        equation, as the FSynth divider will not be updated in this case. */
     && ((m_pCurrentMode->ModeParams.FrameRate >= 50000
          && pModeLine->ModeParams.FrameRate >= 50000)
         || (m_pCurrentMode->ModeParams.FrameRate < 50000
             && pModeLine->ModeParams.FrameRate < 50000))
     )
  {
    g_pIOS->LockResource(m_lock);

    if(m_pCurrentMode->TimingParams.ulPixelClock != pModeLine->TimingParams.ulPixelClock)
    {
      DEBUGF2(2,("CSTmVTG::RequestModeUpdate updating FSynth\n"));
      m_bUpdateClockFrequency = true;
    }

    m_pPendingMode = pModeLine;

    g_pIOS->UnlockResource(m_lock);
    DEBUGF2(2,("CSTmVTG::RequestModeUpdate out\n"));
    return true;
  }

  DEBUGF2(2,("CSTmVTG::RequestModeUpdate mode not compatible\n"));

  return false;
}


void CSTmVTG::GetHSyncPositions(int                        outputid,
                                const stm_timing_params_t &TimingParams,
                                ULONG                     *ulStart,
                                ULONG                     *ulStop)
{
  DENTRY();

  long clocksperline;

  if(m_bDoubleClocked)
    clocksperline = TimingParams.PixelsPerLine*2;
  else
    clocksperline = TimingParams.PixelsPerLine;

  long start;
  long stop;

  if (m_syncType[outputid] == STVTG_SYNC_TIMING_MODE && !TimingParams.HSyncPolarity)
  { // HSync is negative
    DEBUGF2(2,("CSTmVTG::GetHSyncPositions: syncid = %d negative\n",outputid));
    start = TimingParams.HSyncPulseWidth;
    if(m_bDoubleClocked)
      start *= 2;

    stop = 0;
  }
  else
  { // HSync is positive
    DEBUGF2(2,("CSTmVTG::GetHSyncPositions: syncid = %d positive\n",outputid));
    start = 0;
    stop  = TimingParams.HSyncPulseWidth;
    if(m_bDoubleClocked)
      stop *= 2;
  }

  long offset = m_HSyncOffsets[outputid];

  if(m_bDoubleClocked)
    offset *= 2;

  /*
   * Limit the offset to half a line; in reality the offsets are only going to
   * be a handful of clocks so this is being a bit paranoid.
   */
  if(offset >= (LONG)(clocksperline/2))
    offset = (LONG)(clocksperline/2) - 1L;

  if(offset <= -((LONG)(clocksperline/2)))
    offset = -((LONG)(clocksperline/2)) + 1L;

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

  DEBUGF2(2,("CSTmVTG::GetHSyncPositions: syncid = %d start = %lu stop = %lu\n",outputid,start,stop));

  *ulStart = start;
  *ulStop  = stop;
  DEXIT();
}
