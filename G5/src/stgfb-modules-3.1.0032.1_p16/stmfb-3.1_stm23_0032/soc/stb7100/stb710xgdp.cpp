/***********************************************************************
 *
 * File: soc/stb7100/stb710xgdp.cpp
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Gamma/GenericGammaReg.h>

#include "stb710xgdp.h"


CSTb710xGDP::CSTb710xGDP(stm_plane_id_t GDPid,
                         ULONG baseAddr): CGammaCompositorGDP(GDPid, baseAddr, 0)
{
  DENTRY();

  m_ulMaxHSrcInc = m_fixedpointONE;   // no downscale
  m_ulMinHSrcInc = m_fixedpointONE/8; // upscale 8x
  m_ulMaxVSrcInc = m_fixedpointONE;   // No vertical scaling
  m_ulMinVSrcInc = m_fixedpointONE;

  DENTRY();
}

/*
 * STb7109Cut3 is somewhat different, it support vertical resize
 * with programmable filters, flicker filtering and CLUT bitmap formats.
 */

CSTb7109Cut3GDP::CSTb7109Cut3GDP(stm_plane_id_t GDPid,
                                 ULONG baseAddr): CGammaCompositorGDP(GDPid, baseAddr, 0, true)
{
  DENTRY();

  m_bHasVFilter       = true;
  m_bHasFlickerFilter = true;

  m_ulMaxHSrcInc = m_fixedpointONE*2; // downscale to 1/2
  m_ulMinHSrcInc = m_fixedpointONE/8; // upscale 8x
  m_ulMaxVSrcInc = m_fixedpointONE*2; // downscale to 1/2
  m_ulMinVSrcInc = m_fixedpointONE/8;

  /*
   * STBus transaction settings, taken from ST reference drivers.
   */
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_PAS) ,2); // 256byte page
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAOS),5); // Max opcode = 32bytes
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MIOS),3); // Min opcode = 8bytes
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MACS),3); // Max chunk size = 8*max opcode size
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAMS),0); // Max message size = chunk size

  DENTRY();
}
