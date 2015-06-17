/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2011-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "DisplaySource.h"
#include "SourceInterface.h"


CSourceInterface::CSourceInterface ( uint32_t interfaceID, CDisplaySource * pSource )
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Create Source Interface %p with Id = %d", this, interfaceID );

  m_interfaceID     = interfaceID;
  m_pDisplaySource  = pSource;
  m_interfaceHwId   = -1;
  m_bIsSuspended    = false;
  m_bIsFrozen       = false;

  // Create the required source interface
  TRC( TRC_ID_MAIN_INFO, "Source Interface = %p created", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSourceInterface::~CSourceInterface(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Source Interface %p Destroyed", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSourceInterface::Freeze(void)
{
  m_bIsFrozen = true;
}

void CSourceInterface::Suspend(void)
{
  m_bIsSuspended = true;
}

void CSourceInterface::Resume(void)
{
  m_bIsSuspended = false;
  m_bIsFrozen    = false;
}
