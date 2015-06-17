/***********************************************************************
 *
 * File: display/ip/displaytiming/stmfsynth.cpp
 * Copyright (c) 2005-2011 STMicroelectronics Limited.
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

#include "stmfsynth.h"


CSTmFSynth::CSTmFSynth(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_refClock    = STM_CLOCK_REF_30MHZ;
  m_refError    = 0;
  m_adjustment  = 0;
  m_divider     = 1;

  vibe_os_zero_memory(&m_CurrentTiming, sizeof(stm_clock_fsynth_timing_t  ));

  m_pSlavedFSynth = 0;
  m_IsSlave       = false;
  m_NominalOutputFrequency = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmFSynth::~CSTmFSynth()
{
  delete [] m_pSlavedFSynth;
}


bool CSTmFSynth::SetSlave(bool IsSlave)
{
    m_IsSlave   = IsSlave;
    return true;
}


bool CSTmFSynth::RegisterSlavedFSynth(CSTmFSynth *pFSynth)
{
  uint32_t index;

  TRC( TRC_ID_MAIN_INFO, "pFSynth = %p", pFSynth );

  if(m_IsSlave || !pFSynth)
    return false;

  if(!m_pSlavedFSynth)
  {
    if(!(m_pSlavedFSynth = new CSTmFSynth *[MAX_SLAVED_FSYNTH]))
        return false;
    for(index=0; index < MAX_SLAVED_FSYNTH; index++)
        m_pSlavedFSynth[index] = 0L;
  }

  for(index=0; index < MAX_SLAVED_FSYNTH; index++)
  {
      if(m_pSlavedFSynth[index] == 0L)
      {
        m_pSlavedFSynth[index] = pFSynth;
        pFSynth->SetSlave(true);
        TRC( TRC_ID_MAIN_INFO, "%p is now slave of %p", pFSynth, this );
        return true;
      }
  }

  return false;
}


void CSTmFSynth::UnRegisterSlavedFSynth(CSTmFSynth *pFSynth)
{
  uint32_t index;

  TRC( TRC_ID_MAIN_INFO, "pFSynth = %p", pFSynth );

  if (!m_pSlavedFSynth)
    return;

  for(index=0; index < MAX_SLAVED_FSYNTH; index++)
  {
    if(m_pSlavedFSynth[index] == pFSynth)
    {
      pFSynth->SetSlave(false);
      m_pSlavedFSynth[index] = 0;
    }
  }
}


uint32_t CSTmFSynth::AdjustFrequency(uint32_t ulFrequency, int32_t adjustment)
{
  int32_t delta;

  adjustment += m_refError;

  /*             a
   * F = f + --------- * f = f + d
   *          1000000
   *
   *         a
   * d = --------- * f
   *      1000000
   *
   * where:
   *   f - nominal frequency
   *   a - adjustment in ppm (parts per milion)
   *   F - frequency to be set in synthesizer
   *   d - delta (difference) between f and F
   */
  if (adjustment < 0) {
          /* Div64 operates on unsigned values... */
          delta = -1;
          adjustment = -adjustment;
  } else {
          delta = 1;
  }

  /* 500000 ppm is 0.5, which is used to round up values */
  delta *= (int)vibe_os_div64((uint64_t)ulFrequency * (uint64_t)adjustment + 500000ULL, 1000000ULL);

  return ulFrequency + delta;
}


bool CSTmFSynth::Start(uint32_t ulFrequency)
{
  uint32_t index;
  stm_clock_fsynth_timing_t timing;

  uint32_t f = AdjustFrequency(ulFrequency*m_divider, 0);
  if(!SolveFsynthEqn(f, &timing))
  {
    TRC( TRC_ID_ERROR, "Unable to solve fsynth equation for %uHz",f );
    return false;
  }

  m_NominalOutputFrequency = ulFrequency;
  m_CurrentTiming          = timing;
  m_adjustment             = 0;

  TRC( TRC_ID_MAIN_INFO, "setting frequency %uHz for divider %d",ulFrequency,m_divider );
  ProgramClock();

  if (!m_IsSlave && m_pSlavedFSynth)
  {
    for(index=0; index < MAX_SLAVED_FSYNTH; index++)
    {
      if(m_pSlavedFSynth[index])
      {
        if(!m_pSlavedFSynth[index]->Start(ulFrequency))
            return false;
      }
    }
  }

  return true;
}


void CSTmFSynth::Stop()
{
  uint32_t index;
  m_adjustment         = 0;
  m_CurrentTiming.fout = 0;

  if (!m_IsSlave && m_pSlavedFSynth)
  {
    for(index=0; index < MAX_SLAVED_FSYNTH; index++)
    {
      if(m_pSlavedFSynth[index])
        m_pSlavedFSynth[index]->Stop();
    }
  }
}


void CSTmFSynth::SetDivider(int value)
{
  uint32_t index;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "setting divider %d",value );

  m_divider = value;

  if (!m_IsSlave && m_pSlavedFSynth)
  {
    for(index=0; index < MAX_SLAVED_FSYNTH; index++)
    {
      if(m_pSlavedFSynth[index])
        m_pSlavedFSynth[index]->SetDivider(value);
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmFSynth::SetClockReference(stm_clock_ref_frequency_t  refClock, int32_t error_ppm)
{
  uint32_t index;

  m_refClock = refClock;
  m_refError = error_ppm;

  if (!m_IsSlave && m_pSlavedFSynth)
  {
    for(index=0; index < MAX_SLAVED_FSYNTH; index++)
    {
      if(m_pSlavedFSynth[index])
        m_pSlavedFSynth[index]->SetClockReference(refClock, error_ppm);
    }
  }
}


bool CSTmFSynth::SetAdjustment(int ppm)
{
  if(m_CurrentTiming.fout != 0)
  {
    stm_clock_fsynth_timing_t timing;

    if(ppm< -500 || ppm > 500)
    {
      TRC( TRC_ID_ERROR, "Adjustment %dppm out of range",ppm );
      return false;
    }

    uint32_t f = AdjustFrequency(m_NominalOutputFrequency*m_divider,ppm);
    if(!SolveFsynthEqn(f, &timing))
    {
      TRC( TRC_ID_ERROR, "Unable to solve fsynth equation for %uHz",f );
      return false;
    }

    m_adjustment    = ppm;
    m_CurrentTiming = timing;
    ProgramClock();
    return true;
  }

  return false;
}
