/***********************************************************************
 *
 * File: soc/sti7108/sti7108gdp.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7108GDP_H
#define _STi7108GDP_H

#include <Gamma/GammaCompositorGDP.h>
#include <Gamma/GenericGammaReg.h>

class CSTi7108GDP: public CGammaCompositorGDP
{
public:
  CSTi7108GDP(stm_plane_id_t GDPid, ULONG baseAddr): CGammaCompositorGDP(GDPid, baseAddr, 0, true)
  {
    m_bHasVFilter       = true;
    m_bHasFlickerFilter = true;

    m_ulMaxHSrcInc = m_fixedpointONE*4; // downscale to 1/4
    m_ulMinHSrcInc = m_fixedpointONE/8; // upscale 8x
    /* in reality, the hardware can downscale to 1/4 (vertical filter). This
       is achieved by the VF skipping lines for scales < 1/2, but the result
       just looks horrible. As such, we limit the downscale to 1/2 and any
       additional downscale is achieved by modifying the pitch. The question
       remains why the result looks (much!) better like that, as essentially
       the same should be happening in both cases. There is a bit in the
       vertical rescale engine to prevent the line skipping from happening for
       downscales < 1/2, but it does not seem to make any difference, and even
       if it did it would incur a non-neglegible increase in bandwidth as in
       that case the full plane has to be read, i.e. not doing the line skip
       will tremenduously increase the bandwidth requirement.
       As we can easily achieve bigger vertical rescales by adjusting the
       pitch, I would suggest we just continue doing it like this for the
       foreseeable future... */
    m_ulMaxVSrcInc = m_fixedpointONE*2; // downscale to 1/2
    m_ulMinVSrcInc = m_fixedpointONE/8;

    m_bHas4_13_precision = true;

    /*
     * TODO: Check 7108 settings with validation code.
     */
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_PAS) ,6); // 4K page
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAOS),5); // Max opcode = 32bytes
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MIOS),3); // Min opcode = 8bytes
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MACS),3); // Max chunk size = 8*max opcode size
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAMS),0); // Max message size = chunk size
  }

private:
  CSTi7108GDP(const CSTi7108GDP&);
  CSTi7108GDP& operator=(const CSTi7108GDP&);
};

#endif // _STi7108GDP_H
