/***********************************************************************
 *
 * File: soc/sti7200/sti7200gdp.cpp
 * Copyright (c) 2007,2008 STMicroelectronics Limited.
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

#include "sti7200gdp.h"


CSTi7200LocalGDP::CSTi7200LocalGDP(stm_plane_id_t GDPid,
                                   ULONG          baseAddr): CGammaCompositorGDP(GDPid, baseAddr, 0, true)
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
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_PAS) ,6); // 4K page
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAOS),5); // Max opcode = 32bytes
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MIOS),3); // Min opcode = 8bytes
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MACS),3); // Max chunk size = 8*max opcode size
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAMS),0); // Max message size = chunk size

  DEXIT();
}


///////////////////////////////////////////////////////////////////////////////
//
CSTi7200Cut2LocalGDP::CSTi7200Cut2LocalGDP(stm_plane_id_t GDPid,
                                           ULONG          baseAddr): CSTi7200LocalGDP(GDPid, baseAddr)
{
  DENTRY();

  /*
   * Override the downscale for Cut2, we can go down to 1/4 like the 7111
   */
  m_ulMaxHSrcInc = m_fixedpointONE*4;
  /* in reality, the hardware can downscale to 1/4 (vertical filter). This
     is achieved by the VF skipping lines for scales < 1/2, but the result
     just looks horrible. As such, we limit the downscale to 1/2 and any
     additional downscale is achieved by modifying the pitch. The question
     remains why the result looks (much!) better like that, as essentially
     the same should be happening in both cases.
     Note: As opposed to later SoCs the bit to prevent the rescale engine from
     skipping lines does not seem to exist in the 7200. As we do not make
     use of it this is not something to take care of. */
  m_ulMaxVSrcInc = m_fixedpointONE*2;

  DEXIT();
}


/******************************************************************************
 * GDP1 for the Remote display pipeline. It has smaller line buffers,
 * than the GDPs on the Local display, otherwise they are functionally the
 * same.
 */
CSTi7200RemoteGDP::CSTi7200RemoteGDP(stm_plane_id_t GDPid, ULONG baseAddr): CSTi7200LocalGDP(GDPid, baseAddr)
{
  /*
   * Restrict the input image width to 960pixels.
   *
   * Note: that we still allow 1080lines.
   */
  m_capabilities.ulMaxWidth = 960;
}
