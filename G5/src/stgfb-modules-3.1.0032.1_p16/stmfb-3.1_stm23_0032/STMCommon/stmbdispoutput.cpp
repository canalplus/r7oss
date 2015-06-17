/***********************************************************************
 *
 * File: STMCommon/stmbdispoutput.cpp
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

#include <Generic/DisplayDevice.h>

#include "stmblitter.h"
#include "stmbdispregs.h"
#include "stmbdispoutput.h"
#include "stmvirtplane.h"


CSTmBDispOutput::CSTmBDispOutput(CDisplayDevice *pDev,
                                 COutput        *pMasterOutput,
                                 ULONG          *pBDispBaseAddr,
                                 ULONG           BDispPhysAddr,
                                 ULONG           CQOffset,
                                 int             qNumber,
                                 int             qPriority): COutput(pDev,0)
{
  DENTRY();
  DEBUGF2(2,("CSTmBDispOutput::CSTmBDispOutput pDev = %p  master output = %p\n",pDev,pMasterOutput));

  m_pMasterOutput          = pMasterOutput;
  m_ulTimingID             = pMasterOutput->GetTimingID();
  m_pBDispReg              = pBDispBaseAddr;
  m_pCQReg                 = pBDispBaseAddr+(CQOffset>>2);
  m_ulStaticTargetPhysAddr = BDispPhysAddr+BDISP_STBA1;
  
  m_ulQueueID       = qNumber;
  m_ulQueuePriority = qPriority;

  m_buffers[0]      = m_buffers[1] = 0;
  m_bufferSize      = 0;
  m_bufferFormat    = SURF_ARGB8888;
  m_activePlanes    = 0;
  m_lock            = 0;

  m_background      = 0xff006000;

  m_nFrontBuffer    = 0;
  m_bBuffersValid   = false;

  g_pIOS->ZeroMemory(m_pPlanes,sizeof(m_pPlanes));
  g_pIOS->ZeroMemory(&m_systemNodes,sizeof(m_systemNodes));
  g_pIOS->ZeroMemory(&m_fastNodeTemplate,sizeof(m_fastNodeTemplate));
  g_pIOS->ZeroMemory(&m_complexNodeTemplate,sizeof(m_complexNodeTemplate));

  m_complexNodeTemplate.BLT_CIC = (BLIT_CIC_GROUP2  | BLIT_CIC_GROUP3  |
                                   BLIT_CIC_GROUP4  | BLIT_CIC_GROUP6  |
                                   BLIT_CIC_GROUP7  | BLIT_CIC_GROUP8  |
                                   BLIT_CIC_GROUP9  | BLIT_CIC_GROUP11 |
                                   BLIT_CIC_GROUP12 | BLIT_CIC_GROUP14);

  m_complexNodeTemplate.BLT_INS = (BLIT_INS_SRC1_MODE_MEMORY |
                                   BLIT_INS_SRC2_MODE_MEMORY);
  m_complexNodeTemplate.BLT_SAR = BLIT_SAR_TBA_1;

  m_complexNodeTemplate.BLT_CCO = (BLIT_CCO_CLUT_UPDATE_EN | BLIT_CCO_CLUT_EXPAND);
  m_complexNodeTemplate.BLT_FF0 = stm_flicker_filter_coeffs[0];
  m_complexNodeTemplate.BLT_FF1 = stm_flicker_filter_coeffs[1];
  m_complexNodeTemplate.BLT_FF2 = stm_flicker_filter_coeffs[2];
  m_complexNodeTemplate.BLT_FF3 = stm_flicker_filter_coeffs[3];


  m_fastNodeTemplate.BLT_CIC = (BLIT_CIC_GROUP2 | BLIT_CIC_GROUP3 | BLIT_CIC_GROUP14);
  m_fastNodeTemplate.BLT_ACK = BLIT_ACK_BYPASSSOURCE1;
  m_fastNodeTemplate.BLT_SAR = BLIT_SAR_TBA_1;

  DEXIT();
}


CSTmBDispOutput::~CSTmBDispOutput()
{
  DENTRY();
  
  for(int i=0;i<STM_NUM_VIRTUAL_PLANES;i++)
    delete m_pPlanes[i];
    
  g_pIOS->DeleteResourceLock(m_lock);
  g_pIOS->FreeDMAArea(&m_systemNodes);
  
  DEXIT();
}


bool CSTmBDispOutput::Create(void)
{
  struct stm_bdispout_fast_blit_node nodehead = {0};

  DENTRY();

  if(m_ulQueueID<1 || m_ulQueueID>2)
  {
    DEBUGF2(1,(FERROR "Invalid QueueID %lu\n",__PRETTY_FUNCTION__,m_ulQueueID));
    return false;
  }

  m_ulRetriggerInt = BDISP_ITS_CQ_RETRIGGERED<<(m_ulQueueID==1?BDISP_ITS_CQ1_SHIFT:BDISP_ITS_CQ2_SHIFT);

  for(int i=0;i<STM_NUM_VIRTUAL_PLANES;i++)
  {
    stm_plane_id_t planeID = (stm_plane_id_t)((int)OUTPUT_VIRT1<<i);
    m_pPlanes[i] = new CSTmVirtualPlane(planeID,this);
    if((m_pPlanes[i] == 0) || !m_pPlanes[i]->Create())
    {
      DERROR("Failed to create virtual plane");
      return false;
    }

    m_ZOrder[i] = m_pPlanes[i];
  }

  m_lock = g_pIOS->CreateResourceLock();
  if(!m_lock)
  {
    DERROR("Failed to create lock");
    return false;
  }

  g_pIOS->AllocateDMAArea(&m_systemNodes, sizeof(struct stm_bdispout_fast_blit_node)*2, 0, SDAAF_NONE);
  if(!m_systemNodes.pMemory)
  {
    DERROR("failed to allocate system node memory");
    return false;
  }
  
  g_pIOS->MemsetDMAArea(&m_systemNodes, 0, 0, sizeof(struct stm_bdispout_fast_blit_node)*2);

  /*
   * A dummy node to hang the plane composition list off if no mixer background
   * is active. 
   */

  nodehead.BLT_CIC  = BLIT_CIC_GROUP2 | BLIT_CIC_GROUP3 | BLIT_CIC_GROUP14;
  nodehead.BLT_INS  = BLIT_INS_SRC1_MODE_COLOR_FILL;
  CSTmBlitter::SetXY(1,1, &nodehead.BLT_TSZ);
  nodehead.BLT_TTY  = sizeof(ULONG) | BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE;
  nodehead.BLT_S1TY = sizeof(ULONG) | BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE;

  nodehead.BLT_SAR  = BLIT_SAR_TBA_1;

  g_pIOS->MemcpyToDMAArea(&m_systemNodes,0,&nodehead,sizeof(nodehead));

  DEXIT();

  return true;
}


CSTmVirtualPlane *CSTmBDispOutput::GetVirtualPlane(stm_plane_id_t planeID) const
{
  DEBUGF2(2,(FENTRY ": planeID = %d\n",__PRETTY_FUNCTION__,(int)planeID));

  for(int i=0;i<STM_NUM_VIRTUAL_PLANES;i++)
  {
    if(m_pPlanes[i] && (m_pPlanes[i]->GetID() == planeID))
    {
      DEBUGF2(2,(FEXIT ": found virtual plane = %d\n",__PRETTY_FUNCTION__,i+1));
      return m_pPlanes[i];
    }
  }

  DEXIT();
  return 0;
}


bool CSTmBDispOutput::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  struct stm_bdispout_fast_blit_node bkgfill = {0};

  DENTRY();

  /*
   * Setup background fill node
   */
  bkgfill.BLT_CIC = BLIT_CIC_GROUP2 | BLIT_CIC_GROUP3 | BLIT_CIC_GROUP14;
  bkgfill.BLT_INS = BLIT_INS_SRC1_MODE_DIRECT_FILL;

  ULONG pitch;
  ULONG r,g,b,a;
  a = m_background >>24;
  r = (m_background & 0x00ff0000) >> 16;
  g = (m_background & 0x0000ff00) >> 8;
  b = (m_background & 0x000000ff);

  switch(m_bufferFormat)
  {
    case SURF_ARGB8888:
      pitch = pModeLine->ModeParams.ActiveAreaWidth*4;
      bkgfill.BLT_TTY  = pitch | BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE;
      bkgfill.BLT_S1CF = m_background;
      break;
    case SURF_ARGB4444:
      pitch = pModeLine->ModeParams.ActiveAreaWidth*2;
      bkgfill.BLT_TTY  = pitch | BLIT_COLOR_FORM_ARGB4444;
      bkgfill.BLT_S1CF = ((a >>4) << 12) | ((r >>4) << 8) | ((g >>4) << 4) | (b >>4);
      break;
    case SURF_ARGB8565:
      pitch = pModeLine->ModeParams.ActiveAreaWidth*3;
      bkgfill.BLT_TTY  = pitch | BLIT_COLOR_FORM_ARGB8565 | BLIT_TY_FULL_ALPHA_RANGE;
      bkgfill.BLT_S1CF = (a << 16) | ((r >>3) << 11) | ((g >>2) << 5) | (b >>3);
      break;
    default:
      DERROR("GDMA Buffer format not currently supported");
      return false;
  }

  if((pitch*pModeLine->ModeParams.ActiveAreaHeight) > m_bufferSize)
  {
    DERROR("GDMA buffers are too small for display mode");
    return false;
  }
  
  bkgfill.BLT_S1TY = bkgfill.BLT_TTY;
  bkgfill.BLT_TXY  = bkgfill.BLT_S1XY = 0;

  CSTmBlitter::SetXY(pModeLine->ModeParams.ActiveAreaWidth,
                     pModeLine->ModeParams.ActiveAreaHeight,
                     &bkgfill.BLT_TSZ);

  bkgfill.BLT_SAR  = BLIT_SAR_TBA_1;

  m_fastNodeTemplate.BLT_TTY    = bkgfill.BLT_TTY;
  m_complexNodeTemplate.BLT_TTY = bkgfill.BLT_TTY;
  m_complexNodeTemplate.BLT_CWS = bkgfill.BLT_TSZ;
  
  g_pIOS->MemcpyToDMAArea(&m_systemNodes,sizeof(struct stm_bdispout_fast_blit_node),&bkgfill,sizeof(bkgfill));
  
  COutput::Start(pModeLine, tvStandard);

  DEXIT();
  return true;
}


bool CSTmBDispOutput::Stop(void)
{
  DENTRY();

  WriteCQReg(BDISP_CQ_TRIG_CTL, 0);

  m_bBuffersValid = false;
  m_nFrontBuffer  = 0;

  COutput::Stop();  

  DEXIT();

  return true;
}


void CSTmBDispOutput::UpdateHW(void)
{
  register bool isTopFieldOnDisplay = (m_pMasterOutput->GetCurrentVTGVSync() == STM_TOP_FIELD);
  register bool isDisplayInterlaced = (m_pCurrentMode->ModeParams.ScanType == SCAN_I);

  /*
   * Keep the stack small for the case where this is called from interrupt 
   * context. We know that this function cannot be re-entered.
   */
  static int       nip;
  static DMA_Area *lastnode;
  static ULONG     lastnodeoffset;
  static ULONG     firstnodeoffset;
  
  /*
   * This is all grossly simplified at the moment because we only support
   * progressive content onto a progressive GDMA buffer that is a size match
   * for the output mode - so if the output is interlaced it is an easy 
   * presentation. Oh and we don't use the tile ram which would really 
   * complicate matters!
   */

  if(isDisplayInterlaced && !isTopFieldOnDisplay)
    return;

  /*
   * Sort out head of display list, one node to rule them all...
   *
   * Start with a background fill if active, otherwise use the dummy node as
   * the composition list head.
   */
  lastnode = &m_systemNodes;
  if(m_activePlanes & OUTPUT_BKG)
    firstnodeoffset = lastnodeoffset = sizeof(struct stm_bdispout_fast_blit_node);
  else
    firstnodeoffset = lastnodeoffset = 0;

  for(int i=0;i<STM_NUM_VIRTUAL_PLANES;i++)
  {
    DMA_Area *nodes = m_ZOrder[i]->UpdateHW(isDisplayInterlaced, true, m_pMasterOutput->GetCurrentVSyncTime());
    if((m_activePlanes & (ULONG)m_ZOrder[i]->GetID()) && nodes && nodes->pData)
    {
      DEBUGF2(3,("%s: composing plane %d\n",__PRETTY_FUNCTION__,m_ZOrder[i]->GetID()));
      /*
       * Link these nodes into the blitter display list
       */
      nip = nodes->ulPhysical+sizeof(ULONG);
      g_pIOS->MemcpyToDMAArea(lastnode, lastnodeoffset, &nip, sizeof(ULONG));
      lastnodeoffset = *((ULONG *)nodes->pData);
      lastnode = nodes;
    }
  }

  nip = 0;
  g_pIOS->MemcpyToDMAArea(lastnode, lastnodeoffset, &nip, sizeof(ULONG));

  /*
   * Try a memory barrier here, to ensure the blitnode updates have made it to
   * memory, before we program the hardware and it reads the first node.
   * Reread the NULL next instruction pointer we just wrote and flushed from
   * the CPU cache.
   */
  volatile unsigned long *mb = (volatile unsigned long *)(lastnode->pData + lastnodeoffset);
  /*
   * Force cache refill of first word of node and hope the compiler doesn't
   * optimise it out.
   */
  unsigned long val = *mb;

  DEBUGF2(3,("%s *mb = 0x%08lx\n",__PRETTY_FUNCTION__,val));

  /*
   * Setup the target buffer addresses and write the isplay list head into
   * next instruction pointer reg and kick it off.
   */
  register int nextBuffer = (m_nFrontBuffer+1) % 2;
  WriteBDispReg(BDISP_SSBA1, m_buffers[nextBuffer]);
  WriteBDispReg(BDISP_STBA1, m_buffers[nextBuffer]);

  WriteCQReg(BDISP_CQ_TRIG_IP, m_systemNodes.ulPhysical+firstnodeoffset);
  WriteCQReg(BDISP_CQ_TRIG_CTL, (BDISP_CQ_TRIG_CTL_RETRIGGER_ABORT    |
                                 BDISP_CQ_TRIG_CTL_IMMEDIATE          |
                                 BDISP_CQ_TRIG_CTL_QUEUE_EN           ));

  m_bBuffersValid = true;
}


ULONG CSTmBDispOutput::GetCapabilities(void) const
{
  return STM_OUTPUT_CAPS_PLANE_MIXER |
         STM_OUTPUT_CAPS_BLIT_COMPOSITION;
}


bool CSTmBDispOutput::HandleInterrupts(void)
{
  /*
   * Handle Blitter CQ Interrupt
   */
  ULONG intreg = ReadBDispReg(BDISP_ITS) & m_ulRetriggerInt;

  if(intreg == 0)
    return false;

  DEBUGF2(3,("CSTmBDispOutput::HandleInterrupts Blit Queue retriggered before completion\n"));

  /*
   * Clear interrupts for this queue.
   */
  WriteBDispReg(BDISP_ITS,intreg);

  return true;
}


bool CSTmBDispOutput::CanShowPlane(stm_plane_id_t planeID)
{
  return (planeID == OUTPUT_BKG || (planeID >= OUTPUT_VIRT1 && planeID <= OUTPUT_VIRT15));
}


bool CSTmBDispOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,(FENTRY ": planeID = %d\n",__PRETTY_FUNCTION__,(int)planeID));

  if (!(planeID == OUTPUT_BKG || (planeID >= OUTPUT_VIRT1 && planeID <= OUTPUT_VIRT15)))
    return false;
  
  g_pIOS->LockResource(m_lock);  
  m_activePlanes |= (ULONG)planeID;
  g_pIOS->UnlockResource(m_lock);

  return true;
}


void CSTmBDispOutput::HidePlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,(FENTRY ": planeID = %d\n",__PRETTY_FUNCTION__,(int)planeID));

  if (!(planeID == OUTPUT_BKG || (planeID >= OUTPUT_VIRT1 && planeID <= OUTPUT_VIRT15)))
    return;
    
  g_pIOS->LockResource(m_lock);  
  m_activePlanes ^= (ULONG)planeID;
  g_pIOS->UnlockResource(m_lock);
}


bool CSTmBDispOutput::SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate)
{
  DEBUGF2(2,(FENTRY ": planeID = %d depth = %d\n",__PRETTY_FUNCTION__,(int)planeID,depth));

  if(depth < 0 || depth >= STM_NUM_VIRTUAL_PLANES)
    return false;

  /*
   * Don't allow the depth to be changed from the default for now,
   * fix this later.
   */
  if(m_ZOrder[depth]->GetID() != planeID)
    return false;
    
  return true;
}


bool CSTmBDispOutput::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const
{
  DEBUGF2(2,(FENTRY ": planeID = %d\n",__PRETTY_FUNCTION__,(int)planeID));

  for(int i=0;i<STM_NUM_VIRTUAL_PLANES;i++)
  {
    if(m_ZOrder[i]->GetID() == planeID)
    {
      *depth = i;
      return true;
    }
  }
  return false;
}


ULONG CSTmBDispOutput::SupportedControls(void) const
{
  return STM_CTRL_CAPS_BACKGROUND | STM_CTRL_CAPS_GDMA_SETUP;
}


void CSTmBDispOutput::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  DEBUGF2(2,(FENTRY ": ctrl = %d ulNewVal = %lu (0x%08lx)\n",__PRETTY_FUNCTION__,(int)ctrl,ulNewVal,ulNewVal));

  switch(ctrl)
  {
    case STM_CTRL_BACKGROUND_ARGB:
      m_background = ulNewVal;
      break;
    case STM_CTRL_GDMA_BUFFER_1:
      m_buffers[0] = ulNewVal;
      break;
    case STM_CTRL_GDMA_BUFFER_2:
      m_buffers[1] = ulNewVal;
      break;
    case STM_CTRL_GDMA_SIZE:
      m_bufferSize = ulNewVal;
      break;
    case STM_CTRL_GDMA_FORMAT:
      if(ulNewVal > SURF_NULL_PAD && ulNewVal < SURF_END)
        m_bufferFormat = (SURF_FMT)ulNewVal;

      break;  
    default:
      return;
  }
}


ULONG CSTmBDispOutput::GetControl(stm_output_control_t ctrl) const
{
  DEBUGF2(2,(FENTRY ": ctrl = %d\n",__PRETTY_FUNCTION__,(int)ctrl));

  switch(ctrl)
  {
    case STM_CTRL_BACKGROUND_ARGB:
      return m_background;
    case STM_CTRL_GDMA_BUFFER_1:
      return m_buffers[0];
    case STM_CTRL_GDMA_BUFFER_2:
      return m_buffers[1];
    case STM_CTRL_GDMA_SIZE:
      return m_bufferSize;
    case STM_CTRL_GDMA_FORMAT:
      return (ULONG)m_bufferFormat;
    default:
      return 0;
  }
}


inline bool CSTmBDispOutput::isRGB(SURF_FMT fmt)
{
  switch(fmt)
  {
    case SURF_RGB565:
    case SURF_ARGB1555:
    case SURF_ARGB4444:
    case SURF_ARGB8888:
    case SURF_RGB888:
    case SURF_ARGB8565:
      return true;
    default:
      break;
  }
  return false;
}


inline bool CSTmBDispOutput::isRasterYUV(SURF_FMT fmt)
{
  switch(fmt)
  {
    case SURF_CRYCB888:
    case SURF_ACRYCB8888:
      return true;
    default:
      break;
  }
  
  return false;
}


inline bool CSTmBDispOutput::hasAlpha(SURF_FMT fmt)
{
  switch(fmt)
  {
    case SURF_ARGB1555:
    case SURF_ARGB4444:
    case SURF_ARGB8888:
    case SURF_ARGB8565:
    case SURF_ACRYCB8888:
    case SURF_ACLUT88:
      return true;
    default:
      break;
  }
  
  return false;
}


/*
 * Virtual plane proxy method to actually create the BDisp nodes to
 * render the plane on the output.
 */
bool CSTmBDispOutput::CreateCQNodes(const stm_display_buffer_t * const pBuffer,
                                    DMA_Area                   &dma_area,
                                    ULONG                       alphaRamp)
{
  ULONG alpha[2];

  if(pBuffer->src.Rect.x <0 || pBuffer->src.Rect.y <0)
    return false;

  /*
   * First check if this will contribute to the final display. If
   * not return true with no nodes created so the buffer will return
   * to the caller as displayed at the proper time.
   */
  if(pBuffer->src.Rect.width  <= 0 ||
     pBuffer->src.Rect.height <= 0 ||
     pBuffer->dst.Rect.width  <= 0 ||
     pBuffer->dst.Rect.height <= 0)
  {
    return true;
  }
  
  long tmp = pBuffer->dst.Rect.x + pBuffer->dst.Rect.width; 
  if(tmp<0 || pBuffer->dst.Rect.x >= (long)m_pCurrentMode->ModeParams.ActiveAreaWidth)
    return true;
  
  tmp = pBuffer->dst.Rect.y + pBuffer->dst.Rect.height;
  if(tmp<0 || pBuffer->dst.Rect.y >= (long)m_pCurrentMode->ModeParams.ActiveAreaHeight)
    return true;

  /*
   * First spot the cases where we can do a SRC1, non-rescaled, non-blended
   * copy, either with or without a colour format conversion.
   */
  if((pBuffer->src.ulFlags     == 0)                       &&
     (pBuffer->src.Rect.width  == pBuffer->dst.Rect.width) &&
     (pBuffer->src.Rect.height == pBuffer->dst.Rect.height))
  {
    if((pBuffer->dst.ulFlags == STM_PLANE_DST_OVERWITE) ||
       (!hasAlpha(pBuffer->src.ulColorFmt) && pBuffer->src.ulConstAlpha >= 255))
    {
      if((isRGB(pBuffer->src.ulColorFmt) && isRGB(m_bufferFormat)) ||
         (isRasterYUV(pBuffer->src.ulColorFmt) && isRasterYUV(m_bufferFormat)))
      {
        return CreateFastNode(pBuffer,dma_area);
      }
    }
  }

  alpha[0] = ((alphaRamp & 0xff)+1)/2;
  alpha[1] = (((alphaRamp>>8) & 0xff)+1)/2;

  /*
   * TODO: !
   */
  return false;
}


bool CSTmBDispOutput::CreateFastNode(const stm_display_buffer_t * const pBuffer,
                                     DMA_Area                   &dma_area)
{
  struct stm_bdispout_fast_blit_node node = m_fastNodeTemplate;
  stm_blitter_surface_t surf;
  stm_plane_crop_t srcrect,dstrect;
  
  g_pIOS->AllocateDMAArea(&dma_area, sizeof(struct stm_bdispout_fast_blit_node)+sizeof(ULONG), 0, SDAAF_NONE);
  if(!m_systemNodes.pMemory)
  {
    DERROR("failed to allocate node memory");
    return false;
  }

  /*
   * Adjust the rectangles to cope with the destination being partially
   * cropped.
   */
  srcrect = pBuffer->src.Rect;
  dstrect = pBuffer->dst.Rect;

  if(dstrect.x <0)
  {
    srcrect.x -= dstrect.x;
    srcrect.width += dstrect.x;
    dstrect.width += dstrect.x;
    dstrect.x = 0;
  }

  if(dstrect.y <0)
  {
    srcrect.y -= dstrect.y;
    srcrect.height += dstrect.y;
    dstrect.height += dstrect.y;
    dstrect.y = 0;
  }

  long tmp = dstrect.x + dstrect.width - 1;
  if(tmp >= (long)m_pCurrentMode->ModeParams.ActiveAreaWidth)
  {
    tmp = tmp - m_pCurrentMode->ModeParams.ActiveAreaWidth + 1;
    dstrect.width -= tmp;
    srcrect.width -= tmp;
  }
  
  tmp = dstrect.y + dstrect.height - 1;
  if(tmp >= (long)m_pCurrentMode->ModeParams.ActiveAreaHeight)
  {
    tmp = tmp - m_pCurrentMode->ModeParams.ActiveAreaHeight + 1;
    dstrect.height -= tmp;
    srcrect.height -= tmp;
  }

  DEBUGF2(3,("%s: src (%ld,%ld,%ld,%ld) dst (%ld,%ld,%ld,%ld)\n", __PRETTY_FUNCTION__,
                                                                  srcrect.x,
                                                                  srcrect.y,
                                                                  srcrect.width,
                                                                  srcrect.height,
                                                                  dstrect.x,
                                                                  dstrect.y,
                                                                  dstrect.width,
                                                                  dstrect.height));

  node.BLT_S1BA = pBuffer->src.ulVideoBufferAddr;
  /*
   * Reuse some generic blitter code by partially setting up a graphics
   * surface descriptor.
   */
  surf.format   = pBuffer->src.ulColorFmt;
  surf.ulStride = pBuffer->src.ulStride;
  CSTmBlitter::SetBufferType(surf, &node.BLT_S1TY); 
  CSTmBlitter::SetXY(dstrect.width, dstrect.height, &node.BLT_TSZ);
  CSTmBlitter::SetXY(dstrect.x, dstrect.y, &node.BLT_TXY);
  CSTmBlitter::SetXY(srcrect.x, srcrect.y, &node.BLT_S1XY);

  if(surf.format == m_bufferFormat)
    node.BLT_INS = BLIT_INS_SRC1_MODE_DIRECT_COPY;
  else
    node.BLT_INS = BLIT_INS_SRC1_MODE_MEMORY;
  
  ULONG lastnodeoffset = sizeof(ULONG); /* Only one node */
  g_pIOS->MemcpyToDMAArea(&dma_area, 0, &lastnodeoffset, sizeof(ULONG));
  g_pIOS->MemcpyToDMAArea(&dma_area, sizeof(ULONG), &node, sizeof(node));

  return true;
}
