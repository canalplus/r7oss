/***********************************************************************
 *
 * File: display/ip/sync/stmsync.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <display/generic/DisplayDevice.h>

#include <vibe_debug.h>
#include "stmsync.h"


CSTmSync::CSTmSync (CDisplayDevice* pDev, uint32_t ulRegOffset)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  m_pSyncCtrl    = (uint32_t*)pDev->GetCtrlRegisterBase()+(ulRegOffset >> 2);
  m_bIsSuspended = false;
  m_bIsDisabled = false;

  vibe_os_zero_memory( &m_CurrentMode, sizeof( m_CurrentMode ));
  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
  m_ulOutputFormat = 0;
}


CSTmSync::~CSTmSync (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

}


bool CSTmSync::Stop (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  Disable ();

  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
  m_ulOutputFormat = 0;

  return true;
}


void CSTmSync::Suspend (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  if(m_bIsSuspended)
    return;

  if(!m_bIsDisabled)
  {
    /*
     * Do not call Stop() here to keep Current Mode/Format valid for Resume
     */
    this->Disable();
  }

  m_bIsSuspended = true;
}


void CSTmSync::Resume (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  if(!m_bIsSuspended)
    return;

  if(!m_bIsDisabled)
  {
    this->Start(&m_CurrentMode, m_ulOutputFormat);
  }

  m_bIsSuspended = false;
}
