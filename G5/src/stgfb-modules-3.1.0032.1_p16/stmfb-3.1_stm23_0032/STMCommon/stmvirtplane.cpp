/***********************************************************************
 *
 * File: STMCommon/stmvirtplane.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include "Generic/IOS.h"
#include "Generic/IDebug.h"
#include "Generic/DisplayPlane.h"

#include "stmvirtplane.h"
#include "stmbdispoutput.h"


static const SURF_FMT surfaceFormats[] = {
  SURF_RGB565,
  SURF_ARGB1555,
  SURF_ARGB4444,
  SURF_ARGB8888,
  SURF_RGB888,
  SURF_ARGB8565,
  SURF_YCBCR422R,
  SURF_CRYCB888,
  SURF_ACRYCB8888,
  SURF_CLUT8,
  SURF_ACLUT88
};


CSTmVirtualPlane::CSTmVirtualPlane(stm_plane_id_t id, CSTmBDispOutput *pOutput): CDisplayPlane(id)
{
  DENTRY();

  m_pBDispOutput = pOutput;
  m_ulAlphaRamp = 0x0000ff00;

  m_fixedpointONE = 1L<<10;
  m_ulMaxHSrcInc  = 16*m_fixedpointONE;
  m_ulMaxVSrcInc  = 16*m_fixedpointONE;
  m_ulMinHSrcInc  = m_fixedpointONE/16;
  m_ulMinVSrcInc  = m_fixedpointONE/16;

  m_capabilities.ulCaps      = (0
                                | PLANE_CAPS_RESIZE
                                | PLANE_CAPS_SRC_COLOR_KEY
                                | PLANE_CAPS_PREMULTIPLED_ALPHA
                                | PLANE_CAPS_GLOBAL_ALPHA
                                | PLANE_CAPS_FLICKER_FILTER
                               );
  m_capabilities.ulControls  = PLANE_CTRL_CAPS_ALPHA_RAMP;
  m_capabilities.ulMinFields = 1;
  m_capabilities.ulMinWidth  = 1;
  m_capabilities.ulMinHeight = 1;
  m_capabilities.ulMaxWidth  = 1920;
  m_capabilities.ulMaxHeight = 1080;
  SetScalingCapabilities(&m_capabilities);

  DEXIT();
}


CSTmVirtualPlane::~CSTmVirtualPlane() {}


bool CSTmVirtualPlane::QueueBuffer(const stm_display_buffer_t * const pBuffer,
                                   const void                 * const user)
{
  struct stm_plane_node nodeEntry = {{0}};

  /*
   * Check we are connected to an output and the user is allowed to queue
   */
  if(!m_pOutput)
    return false;

  if(m_user != user)
    return false;

  if(pBuffer->src.ulFlags & (STM_PLANE_SRC_INTERLACED         |
                             STM_PLANE_SRC_BOTTOM_FIELD_FIRST |
                             STM_PLANE_SRC_REPEAT_FIRST_FIELD |
                             STM_PLANE_SRC_TOP_FIELD_ONLY     |
                             STM_PLANE_SRC_BOTTOM_FIELD_ONLY  |
                             STM_PLANE_SRC_INTERPOLATE_FIELDS |
                             STM_PLANE_SRC_XY_IN_16THS))
  {
    DERROR("Unsupported src flags");
    return false;
  }
  
  if(pBuffer->dst.ulFlags & (STM_PLANE_DST_COLOR_KEY             |
                             STM_PLANE_DST_COLOR_KEY_INV         |
                             STM_PLANE_DST_RESCALE_TO_VIDEO_RANGE))
  {
    DERROR("Unsupported dst flags");
    return false;
  }


  /*
   * OK, we are being very restrictive with this mechanism at the moment,
   * only supporting progressive graphics content and assuming that the 
   * target GDMA buffers of the output are also progressive, irrespective
   * of the actual display mode. It is up to the specific target hardware to
   * do the progressive->interlaced conversion on actual output.
   */
  nodeEntry.info     = pBuffer->info;
  nodeEntry.nodeType = GNODE_PROGRESSIVE;

  if(nodeEntry.info.nfields == 0)
    nodeEntry.info.nfields = 1;

  if(!m_pBDispOutput->CreateCQNodes(pBuffer, nodeEntry.dma_area, m_ulAlphaRamp))
    return false;
  
  AddToDisplayList(nodeEntry);

  return true;
}


stm_plane_caps_t CSTmVirtualPlane::GetCapabilities() const
{
  return m_capabilities;
}


int CSTmVirtualPlane::GetFormats(const SURF_FMT** pData) const
{
  *pData = surfaceFormats;
  return sizeof(surfaceFormats)/sizeof(SURF_FMT);
}


bool CSTmVirtualPlane::SetControl(stm_plane_ctrl_t control, ULONG value)
{
  switch(control)
  {
    case PLANE_CTRL_ALPHA_RAMP:
      m_ulAlphaRamp = value;
      break;
    default:
      return false;
  }
  
  return true;
}


bool CSTmVirtualPlane::GetControl(stm_plane_ctrl_t control, ULONG *value) const
{
  switch(control)
  {
    case PLANE_CTRL_ALPHA_RAMP:
      *value = m_ulAlphaRamp;
      break;
    default:
      return false;
  }
  
  return true;
}


DMA_Area *CSTmVirtualPlane::UpdateHW (bool   isDisplayInterlaced,
                                      bool   isTopFieldOnDisplay,
                                      TIME64 vsyncTime)
{
  static stm_plane_node displayNode;

  if(!m_isActive)
    return 0;

  UpdateCurrentNode(vsyncTime);

  if(m_isPaused || (m_currentNode.isValid && m_currentNode.info.nfields > 1))
    return &m_currentNode.dma_area;

  /*
   * The generic code for updating planes expects to see updates every field
   * on an interlaced display. However, we are not doing that at the moment,
   * instead we are only implementing a progressive destination updated at half
   * the field rate. So we have to lie about the vsync time, adding a field
   * period, so that when the generic code looks another field period ahead
   * (its usual behaviour for updating a field at a time), it returns buffers
   * early enough to be processed by the blit compositor so they get on the
   * display at the right time. It is a shame that we had to put this knowledge
   * here, but it was necessary as the real vsyncTime is required by the ealier
   * call to UpdateCurrentNode. Therefore we couldn't just adjust the incoming
   * time in the caller. 
   */
  if(isDisplayInterlaced)
    vsyncTime += m_pOutput->GetFieldOrFrameDuration();

  if(!PeekNextNodeFromDisplayList(vsyncTime, displayNode))
    return m_currentNode.isValid?&m_currentNode.dma_area:0;

  
  if(!m_isEnabled)
    EnableHW();

  SetPendingNode(displayNode);
  PopNextNodeFromDisplayList();    
  
  return &m_pendingNode.dma_area;
}
