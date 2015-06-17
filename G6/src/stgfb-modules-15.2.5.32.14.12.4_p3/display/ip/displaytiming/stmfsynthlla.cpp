/***********************************************************************
 *
 * File: display/ip/displaytiming/stmfsynthlla.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
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

#include "stmfsynthlla.h"

CSTmFSynthLLA::CSTmFSynthLLA(struct vibe_clk *clk_fs): CSTmFSynth()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_clk_fs  = clk_fs;

  ASSERTF(m_clk_fs,("m_clk_fs should be valid at creation time!'\n"));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmFSynthLLA::~CSTmFSynthLLA() {}


bool CSTmFSynthLLA::Start(uint32_t ulFrequency)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(!CSTmFSynth::Start(ulFrequency))
    return false;

  TRC( TRC_ID_MAIN_INFO, "Starting Clock %s (rate = %d)", m_clk_fs->name, m_CurrentTiming.fout );

  vibe_os_clk_enable(m_clk_fs);
  if(vibe_os_clk_set_rate(m_clk_fs, m_CurrentTiming.fout) < 0)
  {
    TRC( TRC_ID_ERROR, "Failed to set clock rate for %s (rate = %d)", m_clk_fs->name, m_CurrentTiming.fout );
    return false;
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
  return true;
}


void CSTmFSynthLLA::Stop()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_clk_disable(m_clk_fs);

  CSTmFSynth::Stop();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmFSynthLLA::SolveFsynthEqn(uint32_t Fout, stm_clock_fsynth_timing_t *timing) const
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  /* update the outgoing arguments */
  timing->fout = Fout;
  timing->sdiv = 0;
  timing->md   = 0;
  timing->pe   = 0;

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
  return true;
}


bool CSTmFSynthLLA::SetAdjustment(int ppm)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(!CSTmFSynth::SetAdjustment(ppm))
    return false;

  TRC( TRC_ID_MAIN_INFO, "Adjusting Clock %s to %d", m_clk_fs->name, m_CurrentTiming.fout );
  if(vibe_os_clk_set_rate(m_clk_fs, m_CurrentTiming.fout) < 0)
  {
    TRC( TRC_ID_ERROR, "Failed to set clock rate for %s (rate = %d)", m_clk_fs->name, m_CurrentTiming.fout );
    return false;
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
  return true;
}
