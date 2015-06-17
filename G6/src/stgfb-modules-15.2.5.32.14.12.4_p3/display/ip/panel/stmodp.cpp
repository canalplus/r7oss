/***********************************************************************
 *
 * File: display/ip/panel/stmodp.cpp
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
#include <display/generic/DisplayMixer.h>

#include <display/generic/Output.h>

#include "stmodp.h"

CSTmODP::CSTmODP(CDisplayDevice *pDev)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_pDevRegs = (uint32_t*)pDev->GetCtrlRegisterBase();
    m_SyncUpdateFlag = SYNC_UPDATE_REQ_NONE;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmODP::~CSTmODP() {}

void CSTmODP::SetSyncUpdate(SyncUpdateRequestFlags_t update)
{
    m_SyncUpdateFlag |= update;
}

void CSTmODP::ClearSyncUpdate(SyncUpdateRequestFlags_t update)
{
    m_SyncUpdateFlag &= ~(update);
}

