/***********************************************************************
 *
 * File: Gamma/GammaCompositorCursor.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Generic/Output.h>

#include <STMCommon/stmviewport.h>

#include "GenericGammaDefs.h"
#include "GenericGammaReg.h"

#include "GammaCompositorCursor.h"


static const SURF_FMT g_surfaceFormats[] = {
  SURF_CLUT8_ARGB4444_ENTRIES
};

CGammaCompositorCursor::CGammaCompositorCursor(ULONG baseAddr):CGammaCompositorPlane(OUTPUT_CUR)
{
  DENTRY();

  m_CursorBaseAddr  = baseAddr;
  m_pSurfaceFormats = g_surfaceFormats;
  m_nFormats        = sizeof(g_surfaceFormats)/sizeof(SURF_FMT);

  /*
   * There is no rescaling or subpixel positioning on the cursor plane.
   */
  m_fixedpointONE  = 1;
  m_ulMaxHSrcInc   = m_fixedpointONE;
  m_ulMinHSrcInc   = m_fixedpointONE;
  m_ulMaxVSrcInc   = m_fixedpointONE;
  m_ulMinVSrcInc   = m_fixedpointONE;

  m_ScreenX   = 0;
  m_ScreenY   = 0;
  m_GlobalVPO = 0;

  DEXIT();
}


CGammaCompositorCursor::~CGammaCompositorCursor(void) {}


stm_plane_caps_t CGammaCompositorCursor::GetCapabilities(void) const
{
  static const stm_plane_caps_t caps = {
    ulCaps      : PLANE_CAPS_NONE,
    ulControls  : PLANE_CTRL_CAPS_SCREEN_XY,
    ulMinFields :   1,
    ulMinWidth  :   1,
    ulMinHeight :   1,
    ulMaxWidth  : 128,
    ulMaxHeight : 128,
  };

  return caps;
}


bool CGammaCompositorCursor::QueueBuffer(const stm_display_buffer_t *pBuffer, const void *user)
{
  struct stm_plane_node    nodeEntry = {{0}};
  GammaCursorSetup         setup     = {0};
  const stm_mode_line_t   *pCurrentMode;

  if(m_user != user)
      return false;

  /*
   * Don't allow buffers to be queued if the plane is not connected
   * to an output or that output doesn't have a valid display mode initialised
   */
  if(!m_pOutput)
    return false;

  pCurrentMode = m_pOutput->GetCurrentDisplayMode();

  if(!pCurrentMode)
    return false;

  if((pBuffer->src.ulFlags & STM_PLANE_SRC_INTERLACED) == STM_PLANE_SRC_INTERLACED)
    return false;

  if((pBuffer->src.Rect.width != pBuffer->dst.Rect.width) ||
     (pBuffer->src.Rect.height != pBuffer->dst.Rect.height))
    return false;

  if((pBuffer->src.Rect.width > 128) || (pBuffer->src.Rect.height >128))
    return false;

  if((pBuffer->src.ulColorFmt != SURF_CLUT8_ARGB4444_ENTRIES))
    return false;

  nodeEntry.info     = pBuffer->info;
  nodeEntry.nodeType = GNODE_PROGRESSIVE;

  if(nodeEntry.info.nfields == 0)
    nodeEntry.info.nfields = (pCurrentMode->ModeParams.ScanType == SCAN_I)?2:1;

  setup.CTL = (pBuffer->src.ulClutBusAddress==0)?0:CUR_CTL_CLUT_UPDATE;
  setup.CML = pBuffer->src.ulClutBusAddress;
  setup.PMP = pBuffer->src.ulStride;
  setup.PML = pBuffer->src.ulVideoBufferAddr + (pBuffer->src.Rect.y*pBuffer->src.ulStride) + (pBuffer->src.Rect.x);

  LONG maxWidth = 128 - (setup.PML % 16);

  if(pBuffer->src.Rect.width > maxWidth)
    setup.SIZE = maxWidth;
  else
    setup.SIZE = pBuffer->src.Rect.width;

  setup.SIZE |= (pBuffer->src.Rect.height<<16);

  if(pBuffer->dst.ulFlags & STM_PLANE_DST_USE_SCREEN_XY)
  {
    setup.VPO =  STCalculateViewportPixel(pCurrentMode,pBuffer->dst.Rect.x)      |
                (STCalculateViewportLine(pCurrentMode,pBuffer->dst.Rect.y) << 16);
  }
  else
  {
    setup.VPO = 0xffffffff;
  }

  if(!AllocateNodeMemory(nodeEntry, sizeof(GammaCursorSetup)))
  {
    DERROR("failed to alloc top field memory\n");
    return false;
  }

  g_pIOS->MemcpyToDMAArea(&nodeEntry.dma_area, 0, &setup, sizeof(GammaCursorSetup));

  if(!AddToDisplayList(nodeEntry))
  {
    ReleaseNode(nodeEntry);
    return false;
  }

  return true;
}


void CGammaCompositorCursor::UpdateHW(bool isDisplayInterlaced,
                                      bool isTopFieldOnDisplay,
                                      const TIME64 &vsyncTime)
{
  static stm_plane_node displayNode;

  /*
   * The cursor registers do not have to be updated on each field for an
   * interlaced display.
   */
  UpdateCurrentNode(vsyncTime);

  if(m_isPaused || (m_currentNode.isValid && m_currentNode.info.nfields > 1))
  {
    /*
     * We have got a node on display that is going to be there for more than
     * one frame/field, so we do not get another from the queue yet.
     */
    return;
  }

  if(!PeekNextNodeFromDisplayList(vsyncTime, displayNode))
    return;

  /*
   * We need to enable first so that the global VPO can be calculated.
   */
  if(!m_isEnabled)
    EnableHW();

  writeSetup((GammaCursorSetup*)displayNode.dma_area.pData);

  SetPendingNode(displayNode);
  PopNextNodeFromDisplayList();
  return;
}


void CGammaCompositorCursor::writeSetup(const GammaCursorSetup *setup)
{
  WriteCurReg(CUR_CTL_REG_OFFSET , setup->CTL);
  WriteCurReg(CUR_CML_REG_OFFSET , setup->CML);
  WriteCurReg(CUR_PMP_REG_OFFSET , setup->PMP);
  WriteCurReg(CUR_PML_REG_OFFSET , setup->PML);
  WriteCurReg(CUR_SIZE_REG_OFFSET, setup->SIZE);

  if(setup->VPO != 0xffffffff)
    WriteCurReg(CUR_VPO_REG_OFFSET , setup->VPO);

}


void CGammaCompositorCursor::EnableHW(void)
{
  DENTRY();

  if(!m_isEnabled && m_pOutput)
  {
    ULONG y,x;
    const stm_mode_line_t *mode;

    mode = m_pOutput->GetCurrentDisplayMode();

    x = STCalculateViewportPixel(mode, 0);
    y = STCalculateViewportLine(mode, 0);

    WriteCurReg(CUR_AWS_REG_OFFSET, (x | (y << 16)));

    x = STCalculateViewportPixel(mode, mode->ModeParams.ActiveAreaWidth - 1);
    y = STCalculateViewportLine(mode, mode->ModeParams.ActiveAreaHeight - 1);

    WriteCurReg(CUR_AWE_REG_OFFSET, (x | (y << 16)));

    m_GlobalVPO =  STCalculateViewportPixel(mode,m_ScreenX)      |
                  (STCalculateViewportLine(mode,m_ScreenY) << 16);

    WriteCurReg(CUR_VPO_REG_OFFSET , m_GlobalVPO);

    CDisplayPlane::EnableHW();
  }

  DEXIT();
}


bool CGammaCompositorCursor::SetControl(stm_plane_ctrl_t control, ULONG value)
{
  DEBUGF2(2,(FENTRY ": control = %d ulNewVal = %lu (0x%08lx)\n",__PRETTY_FUNCTION__,(int)control,value,value));

  switch(control)
  {
    case PLANE_CTRL_SCREEN_XY:
    {
      const stm_mode_line_t *mode;

      mode = m_pOutput->GetCurrentDisplayMode();

      m_ScreenX = (short)(value&0xffff);
      m_ScreenY = (short)(value>>16);
      m_GlobalVPO =  STCalculateViewportPixel(mode,m_ScreenX)      |
                    (STCalculateViewportLine(mode,m_ScreenY) << 16);

      DEBUGF2(2,(FENTRY ": ScreenXY(%d,%d)\n",__PRETTY_FUNCTION__,(int)m_ScreenX,(int)m_ScreenY));

      if(m_isEnabled)
        WriteCurReg(CUR_VPO_REG_OFFSET , m_GlobalVPO);

      break;
    }
    default:
      return false;
  }

  return true;
}


bool CGammaCompositorCursor::GetControl(stm_plane_ctrl_t control, ULONG *value) const
{
  DEBUGF2(2,(FENTRY ": control = %d\n",__PRETTY_FUNCTION__,(int)control));

  switch(control)
  {
    case PLANE_CTRL_SCREEN_XY:
      *value = ((ULONG)m_ScreenX & 0xffff) | ((ULONG)m_ScreenY<<16);
      break;
    default:
      return false;
  }

  return true;
}


