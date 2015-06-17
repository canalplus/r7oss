/***********************************************************************
 *
 * File: display/ip/displaytiming/stmflexclkgen_fs660.cpp
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

#include "stmflexclkgen_reg.h"
#include "stmflexclkgen_fs660.h"


CSTmFlexClkGenFS660::CSTmFlexClkGenFS660(CDisplayDevice *pDev, uint32_t uFlexClkGenOffset, int generator, int channelId) : CSTmC32_4FS660()
{
    TRC( TRC_ID_MAIN_INFO, "pDev = %p",pDev );

    m_pDevRegs          = (uint32_t*)pDev->GetCtrlRegisterBase();
    m_uClkGenOffset     = uFlexClkGenOffset + STM_FLEX_CLKGEN_GFG + (STM_FLEX_CLKGEN_GFG_SIZE*generator);
    m_Id                = channelId;

    /*
     * We assume that the VCO part of this fsynth block has been configured
     * to produce the required input frequency to the digital part by the
     * overall platform configuration outside of this driver. Therefore we just
     * read that here so we can calculate the correct fsynth parameters.
     *
     * We also assume the channels are not in reset and not in standby, i.e.
     * everything is set ready to go.
     */
    m_VCO_ndiv = (ReadReg(STM_FCG_FS660_NDIV) & STM_FCG_FS660_NDIV_MASK) >> STM_FCG_FS660_NDIV_SHIFT;
}


CSTmFlexClkGenFS660::~CSTmFlexClkGenFS660() {}


void CSTmFlexClkGenFS660::ProgramClock(void)
{
    TRC( TRC_ID_MAIN_INFO, "FSn_CH%d : MD=%p, PE=%p, SDIV=%p, NSB=%d",m_Id, m_CurrentTiming.md, m_CurrentTiming.pe, m_CurrentTiming.sdiv, m_nsb );

    /*
     * We always want MD/PE to take effect immediately, as this is done with a
     * single register write now, just make sure the program enabled is
     * set.
     */
    WriteBitField(STM_FCG_FS660_PRG_ENABLES, m_Id, 1, 1);

    uint32_t chprg = ((m_CurrentTiming.pe   & STM_FCG_FS660_PE_MASK)   << STM_FCG_FS660_PE_SHIFT)
                   | ((m_CurrentTiming.md   & STM_FCG_FS660_MD_MASK)   << STM_FCG_FS660_MD_SHIFT)
                   | ((m_CurrentTiming.sdiv & STM_FCG_FS660_SDIV_MASK) << STM_FCG_FS660_SDIV_SHIFT)
                   | STM_FCG_FS660_NSDIV;

    WriteReg(STM_FCG_FS660_CH0_CFG+(4*m_Id), chprg);
}

