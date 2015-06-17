/***********************************************************************
 *
 * File: stgfb/Gamma/VBIPlane.cpp
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
#include <Generic/DisplayDevice.h>
#include <Generic/Output.h>

#include "GenericGammaDefs.h"
#include "GenericGammaReg.h"

#include "VBIPlane.h"

CVBIPlane::CVBIPlane(CGammaCompositorGDP *linktogdp,
                     ULONG PKZVal): CGammaCompositorGDP(OUTPUT_VBI,
                                                        linktogdp->m_GDPBaseAddr,
                                                        PKZVal,
                                                        linktogdp->m_bHasClut)
{
  DENTRY();

  /*
   * Override the base class initialization so the Create method will link
   * us to the head of the node list currently represented by the GDP we
   * been passed in "linktogdp".
   */
  m_NextGDPNodeOwner = linktogdp;

  /*
   * We need to override our configuration with the values from the GDP we
   * are linking with, which itself may have been overridden by an SoC
   * specific class. This avoids us needing to create SoC specific versions
   * of this class.
   */
  m_bHasVFilter       = linktogdp->m_bHasVFilter;
  m_bHasFlickerFilter = linktogdp->m_bHasFlickerFilter;

  /*
   * Allow the same horizontal scaling as graphics.
   */
  m_ulMaxHSrcInc   = linktogdp->m_ulMaxHSrcInc;
  m_ulMinHSrcInc   = linktogdp->m_ulMinHSrcInc;
  /*
   * Do not allow vertical scaling.
   */
  m_ulMaxVSrcInc   = m_fixedpointONE;
  m_ulMinVSrcInc   = m_fixedpointONE;

  DEXIT();
}


stm_plane_caps_t CVBIPlane::GetCapabilities(void) const
{
  DENTRY();

  stm_plane_caps_t dynamiccaps = CGammaCompositorGDP::GetCapabilities();

  dynamiccaps.ulMinHeight = 1;
  dynamiccaps.ulCaps |= PLANE_CAPS_VBI_POSITIONING;

  if(m_pOutput)
  {
    const stm_mode_line_t* pCurrentMode = m_pOutput->GetCurrentDisplayMode();

    if(pCurrentMode)
    {
      /*
       * Override the maximum height from the video area to the VBI area.
       */
      dynamiccaps.ulMaxHeight = pCurrentMode->ModeParams.FullVBIHeight;

      if(pCurrentMode->ModeParams.ScanType == SCAN_I)
        dynamiccaps.ulMinHeight = 2;

    }
  }

  return dynamiccaps;
}


bool CVBIPlane::QueueBuffer(const stm_display_buffer_t * const pBuffer,
                            const void                 * const user)
{
  stm_display_buffer_t buffer = *pBuffer;

  if(!m_pOutput)
    return false;

  const stm_mode_line_t* pCurrentMode = m_pOutput->GetCurrentDisplayMode();

  if(!pCurrentMode)
    return false;

  /*
   * We need to change the coordinate system, so that (0,0) is the top left
   * corner of the VBI region not the active video area.
   */
  DEBUGF2(3,("%s: x = %ld y = %ld\n",__PRETTY_FUNCTION__,buffer.dst.Rect.x,buffer.dst.Rect.y));
  buffer.dst.Rect.x -= pCurrentMode->ModeParams.ActiveAreaXStart;
  buffer.dst.Rect.y -= pCurrentMode->ModeParams.FullVBIHeight;
  DEBUGF2(3,("%s: x = %ld y = %ld\n",__PRETTY_FUNCTION__,buffer.dst.Rect.x,buffer.dst.Rect.y));

  return CGammaCompositorGDP::QueueBuffer(&buffer,user);
}


bool CVBIPlane::setOutputViewport(GENERIC_GDP_LLU_NODE          &topNode,
                                  GENERIC_GDP_LLU_NODE          &botNode,
                                  const GAMMA_QUEUE_BUFFER_INFO &qbinfo)
{
  if(!CGammaCompositorGDP::setOutputViewport(topNode,botNode,qbinfo))
    return false;

  topNode.GDPn_PPT = botNode.GDPn_PPT = (GDP_PPT_FORCE_ON_MIX1 |
                                         GDP_PPT_FORCE_ON_MIX2);

  return true;
}


void CVBIPlane::createDummyNode(GENERIC_GDP_LLU_NODE *node)
{
  DENTRY();

  CGammaCompositorGDP::createDummyNode(node);
  /*
   * Override the default viewport to be in the VBI region
   */
  node->GDPn_VPO  = 0x00030010;
  node->GDPn_VPS  = 0x00050011;

  DEXIT();
}
