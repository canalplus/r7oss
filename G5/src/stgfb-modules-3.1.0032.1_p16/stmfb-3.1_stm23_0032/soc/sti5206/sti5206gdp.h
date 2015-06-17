/***********************************************************************
 *
 * File: soc/sti5206/sti5206gdp.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi5206GDP_H
#define _STi5206GDP_H

#include <Gamma/GammaCompositorGDP.h>
#include <Gamma/GenericGammaReg.h>

class CSTi5206GDP: public CGammaCompositorGDP
{
public:
  CSTi5206GDP(stm_plane_id_t GDPid, ULONG baseAddr): CGammaCompositorGDP(GDPid, baseAddr, 0, true)
  {
    /*
     * TODO: Check 5206 settings with validation code.
     */
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_PAS) ,6); // 4K page
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAOS),5); // Max opcode = 32bytes
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MIOS),3); // Min opcode = 8bytes
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MACS),3); // Max chunk size = 8*max opcode size
    g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAMS),0); // Max message size = chunk size
  }

private:
  CSTi5206GDP(const CSTi5206GDP&);
  CSTi5206GDP& operator=(const CSTi5206GDP&);
};

#endif // _STi5206GDP_H
