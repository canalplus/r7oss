/***********************************************************************
 *
 * File: display/ip/panel/stmdp.cpp
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

#include "stmdptx.h"

CSTmDPTx::CSTmDPTx(CDisplayDevice *pDev)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_pDevRegs = (uint32_t*)pDev->GetCtrlRegisterBase();

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmDPTx::~CSTmDPTx() {}

