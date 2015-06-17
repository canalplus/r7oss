/***********************************************************************
 *
 * File: display/ip/panel/stmodpdtg.cpp
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

#include <display/ip/panel/stmodpdtg.h>

CSTmDTGOdp::CSTmDTGOdp(CDisplayDevice      *pDev,
                 CSTmODP             *pODP,
                 CSTmFSynth          *pFSynth,
                 bool                 bDoubleClocked,
                 stm_vtg_sync_type_t  refpol): CSTmVTG(pDev, pFSynth, bDoubleClocked, STMDTG_MAX_SYNC_OUTPUTS)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_pDevRegs = (uint32_t*)pDev->GetCtrlRegisterBase();

    m_pODP = pODP;
    m_pFSynth = pFSynth;

    m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
    m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;

    m_bInvertedField = false;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmDTGOdp::~CSTmDTGOdp() {}

bool CSTmDTGOdp::Start(const stm_display_mode_t* pModeLine)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );


    /* Set Pending Mode */
    m_PendingMode = *(stm_display_mode_t*)pModeLine;

    if(m_PendingMode.mode_id == STM_TIMING_MODE_RESERVED)
    {
        TRC( TRC_ID_MAIN_INFO, "Error: a pending mode is being to be installe !!" );
        return false;
    }

    /* Setup Clocks */
    if(m_pFSynth)
    {
        m_pFSynth->Start(m_PendingMode.mode_timing.pixel_clock_freq);
    }

    /* Program Mode */
    DTGSetModeParams();

    /* Enable DTG */
    EnableDTG();

    /* Enable Interrupts */
    EnableInterrupts();

    m_pODP->HardwareUpdate();
    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return true;
}


void CSTmDTGOdp::Stop(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    // Disable interrupts
    DisableInterrupts();
    // Disable DTG
    DisableDTG();

    m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
    m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;

    m_bUpdateOnlyClockFrequency = false;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CSTmDTGOdp::ResetCounters(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDTGOdp::DisableSyncs(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDTGOdp::RestoreSyncs(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

int CSTmDTGOdp::SetInputLockScheme(stm_display_panel_timing_config_t lockparam)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    switch(lockparam.lock_method)
    {
        case STM_PANEL_FREERUN:
                m_LockParams.LockMethod = DTG_FREERUN;
        break;

        case STM_PANEL_FIXED_FRAME_LOCK:
                m_LockParams.LockMethod = DTG_FIXED_FRAME_LOCK;
        break;

        case STM_PANEL_DYNAMIC_FRAME_LOCK:
                m_LockParams.LockMethod = DTG_DYNAMIC_FRAME_LOCK;
        break;

        case STM_PANEL_LOCK_LOAD:
                m_LockParams.LockMethod = DTG_LOCK_LOAD;
        break;

        default:
            return true;
        break;
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return false;
}

