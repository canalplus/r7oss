/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_odpdtg.cpp
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

#include <display/ip/panel/stmodp.h>
#include "mpe41_odpdtg.h"
#include "mpe41_odp_regs.h"

#define DTG_WAIT(x) (vibe_os_stall_execution (x))

 #define BIT_FRAME_LOCK_FREE_RUN        (0L << FRAME_LOCK_MODE_OFFSET)
 #define BIT_FRAME_LOCK_FIXED           (1L << FRAME_LOCK_MODE_OFFSET)
 #define BIT_FRAME_LOCK_DYNAMIC         (2L << FRAME_LOCK_MODE_OFFSET)
 #define BIT_LOCK_SOURCE_MAIN           (0L << LOCK_SOURCE_OFFSET)
 #define BIT_LOCK_SOURCE_PIP            (1L << LOCK_SOURCE_OFFSET)

#define H_CORR_TOL                          16L
#define V_CORR_TOL                          0L

CSTmMPE41DTGOdp::CSTmMPE41DTGOdp(CDisplayDevice      *pDev,
                 uint32_t             uRegOffset,
                 CSTmODP             *pODP,
                 CSTmFSynth          *pFSynth,
                 bool                 bDoubleClocked,
                 stm_vtg_sync_type_t  refpol): CSTmDTGOdp(pDev, pODP, pFSynth, bDoubleClocked, refpol)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_pODP = pODP;
    m_uDTGOffset = uRegOffset;

    /* Enable only VACTIVE_REDGE interrupt for Output Update */
    m_InterruptMask = D_VS_EDGE_MASK;

    /* Disable Interrupt */
    CSTmMPE41DTGOdp::DisableInterrupts();

    /* Disable DTG */
    CSTmMPE41DTGOdp::DisableDTG();

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmMPE41DTGOdp::~CSTmMPE41DTGOdp() {}


/* Interrupt status handling */
uint32_t CSTmMPE41DTGOdp::GetInterruptStatus(void)
{
    uint32_t ulIntStatus  =  DTG32_Read(ODP_DISPLAY_IRQ1_STATUS);
    uint32_t ulIntMode    =  DTG32_Read(ODP_DISPLAY_IRQ1_MODE);
    uint32_t ulSysCtl     =  DTG32_Read(SYNC_CONTROL);
    uint32_t events       =  STM_TIMING_EVENT_NONE;
    bool IsInterlaced     = false;
    bool IsOddField       = false;

    /*
    * Check Interrupt Mode and rite back to clear the interrupts if the interrupt mode is edge-triggered
    */
    if(ulIntMode & m_InterruptMask)
    {
        DTG32_Write(ODP_DISPLAY_IRQ1_STATUS,ulIntStatus);
    }

    ulIntStatus = ulIntStatus & m_InterruptMask;

    m_bSuppressMissedInterruptMessage = false;

    if(ulIntStatus & D_VS_EDGE_MASK_MASK)
    {
        if(m_PendingMode.mode_id != STM_TIMING_MODE_RESERVED)
        {
            if(m_bUpdateOnlyClockFrequency)
            {
                m_pFSynth->Start(m_PendingMode.mode_timing.pixel_clock_freq);
                m_bUpdateOnlyClockFrequency = false;
            }
            // Set New Current Mode
            m_CurrentMode = m_PendingMode;;
            // Invalidate Pending Mode */
            m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
        }
        IsInterlaced = ulSysCtl & ~INTERLACE_EN_OFFSET;
        if(IsInterlaced)
        {
            IsOddField   = ulSysCtl & ~ODD_EN_OFFSET;
            if(IsOddField)
            {
                if(m_bInvertedField)
                    ulSysCtl |= ODD_EN_OFFSET;
                else
                    ulSysCtl &= ~ODD_EN_OFFSET;
                events |= STM_TIMING_EVENT_BOTTOM_FIELD;
            }
            else
            {
                if(m_bInvertedField)
                    ulSysCtl &= ~ODD_EN_OFFSET;
                else
                    ulSysCtl |= ODD_EN_OFFSET;
                events |= (STM_TIMING_EVENT_FRAME | STM_TIMING_EVENT_TOP_FIELD);
            }
            DTG32_Write(SYNC_CONTROL,ulSysCtl);
        }
        else
        {
        events |= STM_TIMING_EVENT_FRAME;
    }
    }

    if(ulIntStatus & D_LINEFLAG1_MASK)
      events |= STM_TIMING_EVENT_LINE;

    return events;
}



void CSTmMPE41DTGOdp::EnableDTG(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /* Enable Control, Data and DTG */
    DTG32_Set(DISPLAY_CTRL, DTG_RUN_EN_MASK | DCNTL_EN_MASK | DDATA_EN_MASK);

    /* Host Update in ODP */
    m_pODP->SetSyncUpdate(SYNC_UPDATE_REQ_ODP);

    m_bDisabled = false;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmMPE41DTGOdp::DisableDTG(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /* Disable Control, Data and DTG */
    DTG32_Clear(DISPLAY_CTRL,  (DTG_RUN_EN_MASK  | DCNTL_EN_MASK | DDATA_EN_MASK));

    m_bDisabled = true;
    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmMPE41DTGOdp::EnableInterrupts(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /* Clear Status */
    DTG32_Set(ODP_DISPLAY_IRQ1_STATUS, m_InterruptMask);
    /* Enable Edge Trigger */
    DTG32_Set(ODP_DISPLAY_IRQ1_MODE, m_InterruptMask);
    /* Enable interrupts */
    DTG32_Set(ODP_DISPLAY_IRQ1_MASK, m_InterruptMask);

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmMPE41DTGOdp::DisableInterrupts(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /* Disable interrupts */
    DTG32_Clear(ODP_DISPLAY_IRQ1_MASK, m_InterruptMask);

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CSTmMPE41DTGOdp::DTGSetModeParams(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /* Set Input Locking Scheme */
    ProgramDTGInputLockingParams();
    /* Update DTG Mode */
    ProgramDTGTimingsParams();
    /* Update DTG Syncs */
    ProgramDTGSyncsParams();

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

#define SUP_IP_REF_RATES                    12
#define H_CORR_TOL_V_ONLY                   0L
#define V_CORR_TOL_V_ONLY                   5L

int CSTmMPE41DTGOdp::ProgramDTGInputLockingParams(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return true;
}


void CSTmMPE41DTGOdp::ProgramDTGSyncsParams(void)
{
    if(m_PendingMode.mode_id == STM_TIMING_MODE_RESERVED)
        return;

    DTG32_Write(DH_TOTAL, m_PendingMode.mode_timing.pixels_per_line);
    DTG32_SetField(DH_HS_WIDTH, DH_HS_WIDTH, m_PendingMode.mode_timing.hsync_width);
    DTG32_Write(DV_TOTAL, m_PendingMode.mode_timing.lines_per_frame);
    DTG32_SetField(DISPLAY_VSYNC, DH_VS_END, m_PendingMode.mode_timing.vsync_width);

    m_pODP->SetSyncUpdate(SYNC_UPDATE_REQ_ODP);
}


void CSTmMPE41DTGOdp::ProgramDTGTimingsParams(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    if(m_PendingMode.mode_id == STM_TIMING_MODE_RESERVED)
        return;

    DTG32_Write(DH_ACTIVE_START, m_PendingMode.mode_params.active_area_start_pixel);
    DTG32_Write(DH_ACTIVE_WIDTH, m_PendingMode.mode_params.active_area_width);
    DTG32_Write(DH_DE_START, m_PendingMode.mode_params.active_area_start_pixel);
    DTG32_Write(DH_DE_END, (m_PendingMode.mode_params.active_area_start_pixel + \
    m_PendingMode.mode_params.active_area_width));

    DTG32_Write(DV_ACTIVE_START, m_PendingMode.mode_params.active_area_start_line);
    DTG32_Write(DV_ACTIVE_LENGTH, m_PendingMode.mode_params.active_area_height);
    DTG32_Write(DV_DE_START, m_PendingMode.mode_params.active_area_start_line);
    DTG32_Write(DV_DE_END, (m_PendingMode.mode_params.active_area_start_line) + \
    m_PendingMode.mode_params.active_area_height);

    m_pODP->SetSyncUpdate(SYNC_UPDATE_REQ_ODP);
    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

