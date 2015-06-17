/***********************************************************************
 *
 * File: soc/sti5206/sti5206videopipes.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Gamma/DEIReg.h>

#include "sti5206videopipes.h"

CSTi5206DEI::CSTi5206DEI (stm_plane_id_t   GDPid,
                          CGammaVideoPlug *plug,
                          ULONG            baseAddr): CDEIVideoPipeV2 (GDPid,
                                                                       plug,
                                                                       baseAddr)
{
  DENTRY();

  /*
   * This is an SD device.
   */
  m_nDEIPixelBufferLength = 720;
  m_nDEIPixelBufferHeight = 576;

  /*
   * TODO: Override the FMD initialization parameters.
   */

  /*
   * motion data allocated from system partition, there is only one LMI on
   * sti5206.
   */
  m_ulMotionDataFlag = SDAAF_NONE;

  /*
   * TODO: confirm if it has the P2I block
   */
  m_bHasProgressive2InterlacedHW = false;

  /*
   * TODO: Get validation settings.
   */
  m_nMBChunkSize          = 0x3; // Validation actually recommend 0xf, this is better for extreme downscales
  m_nRasterOpcodeSize     = 32;
  m_nRasterChunkSize      = 0x7;
  m_nPlanarChunkSize      = 0x3;// Validation do not test this mode so do not have a recommendation

  m_nMotionOpcodeSize     = 32;
  WriteVideoReg (DEI_T3I_MOTION_CTL, (DEI_T3I_CTL_OPCODE_SIZE_32 |
                                      (0x3 << DEI_T3I_CTL_CHUNK_SIZE_SHIFT)));

  DEXIT();
}


bool CSTi5206DEI::Create(void)
{
  DENTRY();

  if (!CDEIVideoPipeV2::Create ())
    return false;

  /*
   * Override again to limit to SD, note this doesn't change the rescaling.
   */
  m_capabilities.ulMaxWidth  = 720;
  m_capabilities.ulMaxHeight = 576;

  DENTRY();

  return true;
}


/******************************************************************************
 *
 */
CSTi5206VDP::CSTi5206VDP(stm_plane_id_t   planeID,
                         CGammaVideoPlug *plug,
                         ULONG            vdpBaseAddr): CDEIVideoPipe(planeID,plug,vdpBaseAddr)
{
  DENTRY();

  m_bHaveDeinterlacer = false;
  m_keepHistoricBufferForDeinterlacing = false;

  DEXIT();
}


bool CSTi5206VDP::Create(void)
{
  DENTRY();

  if (!CDEIVideoPipe::Create ())
    return false;

  /*
   * Override again to limit to SD, note this doesn't change the rescaling.
   */
  m_capabilities.ulMaxWidth  = 720;
  m_capabilities.ulMaxHeight = 576;

  DEXIT();

  return true;
}
