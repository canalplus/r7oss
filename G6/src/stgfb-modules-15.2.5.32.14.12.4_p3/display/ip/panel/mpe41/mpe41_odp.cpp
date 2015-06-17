/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_odp.cpp
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

#include <display/ip/panel/mpe41/mpe41_odp.h>
#include "mpe41_odp_regs.h"

#define ODP_WAIT(x) (vibe_os_stall_execution (x))
#define TrODP(level, ...) TRC( TRC_ID_UNCLASSIFIED, __VA_ARGS__ )

static uint8_t DebugInfo = 3;
static uint8_t DebugErr = 1;

CSTmMPE41ODP::CSTmMPE41ODP(
                 CDisplayDevice *pDev,
                 uint32_t ulODPRegs): CSTmODP(pDev)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_pDevRegs = (uint32_t*)pDev->GetCtrlRegisterBase();
    m_ulODPRegOffset = ulODPRegs;

    CSTmMPE41ODP::Init();

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmMPE41ODP::~CSTmMPE41ODP() {}

void CSTmMPE41ODP::Init(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    ODP32_Set(ODP_RESET_CTRL, ODP_HARD_RESET_MASK);
    /* Wait for Hard reset to be de-asserted for all the modules in ODP */
    while( (ODP32_Read(ODP_RESET_STATUS) & ODP_RESET_STATUS_MASK) == ODP_RESET_STATUS_MASK)
        ODP_WAIT(10);
    ODP32_Clear(ODP_RESET_CTRL, ODP_HARD_RESET_MASK);

    TrODP(DebugInfo, "Hard reset de-asserted for all the modules in ODP.");

    ODP32_Write(ODP_FRAME_RESET_CTRL, 0);
    ODP32_Write(CPF_TX_FRAME_RESET_SEL, 0);

    ODP32_Write(ODP_COMPO_VSYNC_VCOUNT_CTRL, ODP_COMPO_VSYNC_SEL_MASK);
    ODP32_Write(ACT_EARLY_ADJ_CTRL, 0);

    ODP32_SetField(ODP_FRAME_RESET_OFFSET, FRAME_RESET_OFFSET, 0x10);

    /* Bypass LEDBL */
    ODP32_Write(ODP_CTRL, ODP_LEDBL_BYPASS_MASK);

    /* Initial non-zero value Panel config will update accordingly */
    ODP32_Write(PANEL_PWR_UP    , 0xE46F);
    ODP32_Write(PANEL_PWR_DN    , 0xFFFF);
    ODP32_Write(PANEL_PWR_DIV   , 0);

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

uint32_t CSTmMPE41ODP::SetPowerTiming(
                                    stm_display_panel_power_timing_config_t* pTiming)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    uint16_t PwrUpT1Ms, PwrUpT2Ms, PwrDnT1Ms, PwrDnT2Ms;
    uint16_t PwrUpT1Reg, PwrUpT2Reg, PwrDnT1Reg, PwrDnT2Reg;
    uint16_t MaxPwrMs = 0;
    uint8_t  PwrDivReg = 0;

    PwrUpT1Ms = pTiming->pwr_to_de_delay_during_power_on;
    PwrUpT2Ms = pTiming->de_to_bklt_on_delay_during_power_on;
    PwrDnT1Ms = pTiming->bklt_to_de_off_delay_during_power_off;
    PwrDnT2Ms = pTiming->de_to_pwr_delay_during_power_off;

    /* Find Maximum Value */
    if(PwrUpT1Ms > MaxPwrMs)
        MaxPwrMs = PwrUpT1Ms;
    if(PwrUpT2Ms > MaxPwrMs)
        MaxPwrMs = PwrUpT2Ms;
    if(PwrDnT1Ms > MaxPwrMs)
        MaxPwrMs = PwrDnT1Ms;
    if(PwrDnT2Ms > MaxPwrMs)
        MaxPwrMs = PwrDnT2Ms;

    /* Find Divider Value */
    if(MaxPwrMs > 0)
    {
        PwrDivReg = (MaxPwrMs*1000/545-1)/(PANEL_PWR_DN_T1_MASK+1);
    }
    if(PwrDivReg > PANEL_PWR_SEQ_TCLK_TICK_MASK)
    {
        TrODP(DebugErr, "Divider Error! Too big power delay vaue.");
        return STM_OUT_INVALID_VALUE;
    }

    /* Power Timing */
    if(PwrUpT1Ms != 0)
        PwrUpT1Reg = (PwrUpT1Ms*1000/545-1)/(PwrDivReg+1);
    else
        PwrUpT1Reg = 0;

    if(PwrUpT2Ms != 0)
        PwrUpT2Reg = (PwrUpT2Ms*1000/545-1)/(PwrDivReg+1);
    else
        PwrUpT2Reg = 0;

    if(PwrDnT1Ms!=0)
        PwrDnT1Reg = (PwrDnT1Ms*1000/545-1)/(PwrDivReg+1);
    else
        PwrDnT1Reg = 0;

    if(PwrDnT2Ms!=0)
        PwrDnT2Reg= (PwrDnT2Ms*1000/545-1)/(PwrDivReg+1);
    else
        PwrDnT2Reg= 0;

    ODP32_Write(PANEL_PWR_UP, (PwrUpT1Reg|(PwrUpT2Reg<<PANEL_PWR_UP_T2_OFFSET)));
    ODP32_Write(PANEL_PWR_DN, (PwrDnT1Reg|(PwrDnT2Reg<<PANEL_PWR_DN_T2_OFFSET)));
    ODP32_Write(PANEL_PWR_DIV, PwrDivReg);

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return STM_OUT_OK;
}

uint32_t CSTmMPE41ODP::SetForceUpdate(ForceUpdateRequestFlags_t update)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    if (update & FORCE_UPDATE_REQ_ODP)
    {
        ODP32_Write(ODP_PA_REG_UPDATE_CTRL, ODP_PA_FORCE_UPDATE_MASK);
    }
    else
    {
        return STM_OUT_INVALID_VALUE;
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return STM_OUT_OK;
}

uint32_t CSTmMPE41ODP::HardwareUpdate(void)
{
    uint32_t odp_update = 0;

    TRCIN( TRC_ID_MAIN_INFO, "" );

    if (m_SyncUpdateFlag & SYNC_UPDATE_REQ_ODP)
    {
        odp_update |= ODP_PA_SYNC_UPDATE_MASK;
        m_SyncUpdateFlag &= ~(SYNC_UPDATE_REQ_ODP);
    }
    else
    {
        return STM_OUT_INVALID_VALUE;
    }

    ODP32_Write(ODP_PA_REG_UPDATE_CTRL, odp_update);
    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return STM_OUT_OK;
}

