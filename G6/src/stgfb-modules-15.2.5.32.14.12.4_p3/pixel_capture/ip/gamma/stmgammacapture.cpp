/***********************************************************************
 *
 * File: pixel_capture/ip/gamma/stmgammacapture.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display_types.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "capture_defs.h"
#include "capture_regs.h"

#include "stmgammacapture.h"
#include "capture_filters.h"

#define CAP_HFILTER_TABLE_SIZE (NB_HSRC_FILTERS * (NB_HSRC_COEFFS + HFILTERS_ALIGN))
#define CAP_VFILTER_TABLE_SIZE (NB_VSRC_FILTERS * (NB_VSRC_COEFFS + VFILTERS_ALIGN))

//#define CAPTURE_DEBUG_REGISTERS

/*
 * Timout value is calculated as below :
 * timeout = 2 * max VSync duration
 *
 * 2 for interlaced display latancy
 * max VSync duration = 40ms
 */
#define CAPTURE_RELEASE_WAIT_TIMEOUT      80

#define CAPTURE_INPUT_CS_RGB              0x1

static const stm_pixel_capture_input_s stm_pixel_capture_inputs[] = {
  {"stm_input_video1",      CAP_CTL_SOURCE_VID1      , 0},
  {"stm_input_video2",      CAP_CTL_SOURCE_VID2      , 1},
  {"stm_input_mix2",        CAP_CTL_SOURCE_MIX2_YCbCr, 1},
  {"stm_input_mix1",        CAP_CTL_SOURCE_MIX1_YCbCr, 0},
};

static const stm_pixel_capture_format_t c_surfaceFormats[] = {
  STM_PIXEL_FORMAT_YCbCr422R,
  STM_PIXEL_FORMAT_RGB565,
  STM_PIXEL_FORMAT_RGB_8B8B8B_SP,
  STM_PIXEL_FORMAT_ARGB1555,
  STM_PIXEL_FORMAT_ARGB8565,
  STM_PIXEL_FORMAT_ARGB8888,
  STM_PIXEL_FORMAT_ARGB4444,
  STM_PIXEL_FORMAT_YUV_8B8B8B_SP,
};

#if defined(CAPTURE_DEBUG_REGISTERS)
static uint32_t ctx=0;
#endif

CGammaCompositorCAP::CGammaCompositorCAP(const char     *name,
                                         uint32_t        id,
                                         uint32_t        base_address,
                                         uint32_t        size,
                                         const CPixelCaptureDevice *pDev,
                                         const stm_pixel_capture_capabilities_flags_t caps):CCapture(name, id, pDev, caps)
{
  TRC( TRC_ID_MAIN_INFO, "CGammaCompositorCAP::CGammaCompositorCAP %s id = %u", name, id );

  m_pSurfaceFormats = c_surfaceFormats;
  m_nFormats = N_ELEMENTS (c_surfaceFormats);

  m_ulCAPBaseAddress       = base_address;

  m_pCaptureInputs         = stm_pixel_capture_inputs;
  m_pCurrentInput          = m_pCaptureInputs[0];
  m_numInputs              = N_ELEMENTS(stm_pixel_capture_inputs);

  /*
   * Override scaling capabilities this for Gamma Compositor Capture hw.
   */
  m_ulMaxHSrcInc = m_fixedpointONE*8; // downscale to 1/8
  m_ulMaxVSrcInc = m_fixedpointONE*8; // downscale to 1/8
  m_ulMinHSrcInc = m_fixedpointONE;   // upscale not supported by compo capture
  m_ulMinVSrcInc = m_fixedpointONE;   // upscale not supported by compo capture

  /* No Skip Line support for Compositor Capture */
  m_ulMaxLineStep = 1;

  m_HFilter      = (DMA_Area) { 0 };
  m_VFilter      = (DMA_Area) { 0 };

  m_pReg = (uint32_t*)vibe_os_map_memory(m_ulCAPBaseAddress, size);
  ASSERTF(m_pReg,("CGammaCompositorCAP::CGammaCompositorCAP 'unable to map CAPTURE device registers'\n"));

  TRC( TRC_ID_MAIN_INFO, "CGammaCompositorCAP::CGammaCompositorCAP: Register remapping 0x%08x -> 0x%08x", m_ulCAPBaseAddress,(uint32_t)m_pReg );

  TRC( TRC_ID_MAIN_INFO, "CGammaCompositorCAP::CGammaCompositorCAP out" );
}


CGammaCompositorCAP::~CGammaCompositorCAP(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  this->Stop();

  vibe_os_free_dma_area(&m_HFilter);
  vibe_os_free_dma_area(&m_VFilter);

  vibe_os_unmap_memory(m_pReg);
  m_pReg = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CGammaCompositorCAP::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Each filter table contains _two_ complete sets of filters, one for
   * Luma and one for Chroma.
   */
  vibe_os_allocate_dma_area(&m_HFilter, CAP_HFILTER_TABLE_SIZE, 16, SDAAF_NONE);
  if(!m_HFilter.pMemory)
  {
    TRC( TRC_ID_ERROR, "CGammaCompositorCAP::Create 'out of memory'" );
    return false;
  }
  vibe_os_memcpy_to_dma_area(&m_HFilter,0,&capture_HSRC_Coeffs,sizeof(capture_HSRC_Coeffs));

  TRC( TRC_ID_MAIN_INFO, "CGammaCompositorCAP::Create &m_HFilter = %p pMemory = %p pData = %p phys = %08x", &m_HFilter,m_HFilter.pMemory,m_HFilter.pData,m_HFilter.ulPhysical );

  vibe_os_allocate_dma_area(&m_VFilter, CAP_VFILTER_TABLE_SIZE, 16, SDAAF_NONE);
  if(!m_VFilter.pMemory)
  {
    TRC( TRC_ID_ERROR, "CGammaCompositorCAP::Create 'out of memory'" );
    goto exit_error;
  }
  vibe_os_memcpy_to_dma_area(&m_VFilter,0,&capture_VSRC_Coeffs,sizeof(capture_VSRC_Coeffs));

  TRC( TRC_ID_MAIN_INFO, "CGammaCompositorCAP::Create &m_VFilter = %p pMemory = %p pData = %p phys = %08x", &m_VFilter,m_VFilter.pMemory,m_VFilter.pData,m_VFilter.ulPhysical );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

exit_error:
  if(m_HFilter.pMemory)
      vibe_os_free_dma_area(&m_HFilter);

  if(m_VFilter.pMemory)
      vibe_os_free_dma_area(&m_VFilter);

  return false;
}


bool CGammaCompositorCAP::ConfigureCaptureResizeAndFilters(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      GammaCaptureSetup_t                  *pCaptureSetup)
{
  uint32_t ulCtrl = 0;
  uint32_t hw_srcinc;
  unsigned filter_index;

  if(qbinfo.hsrcinc != (uint32_t)m_fixedpointONE)
  {
    TRC( TRC_ID_UNCLASSIFIED, "H Resize Enabled (HSRC 4.13/4.8: %x/%x)", qbinfo.hsrcinc, qbinfo.hsrcinc >> 5 );

    hw_srcinc = (qbinfo.hsrcinc >> 5);

    pCaptureSetup->HSRC = hw_srcinc;

    ulCtrl |= CAP_CTL_EN_H_RESIZE;

    for(filter_index=0;filter_index<N_ELEMENTS(HSRC_index);filter_index++)
    {
      if(hw_srcinc <= HSRC_index[filter_index])
      {
        TRC( TRC_ID_UNCLASSIFIED, "  -> 5-Tap Polyphase Filtering (idx: %d)",  filter_index );

        pCaptureSetup->HFCn = (uint32_t )m_HFilter.pData + (filter_index*HFILTERS_ENTRY_SIZE);

        pCaptureSetup->HSRC |= CAP_HSRC_FILTER_EN;
        break;
      }
    }
  }

  if(qbinfo.vsrcinc != (uint32_t)m_fixedpointONE)
  {
    TRC( TRC_ID_UNCLASSIFIED, "V Resize Enabled (VSRC 4.13/4.8: %x/%x)", qbinfo.vsrcinc, qbinfo.vsrcinc >> 5 );

    hw_srcinc = (qbinfo.vsrcinc >> 5);

    pCaptureSetup->VSRC = hw_srcinc;

    ulCtrl |= CAP_CTL_EN_V_RESIZE;

    if(qbinfo.isInputInterlaced)
    {
      /*
       * When putting progressive content on an interlaced display
       * we adjust the filter phase of the bottom field to start
       * an appropriate distance lower in the source bitmap, based on the
       * scaling factor. If the scale means that the bottom field
       * starts >1 source bitmap line lower then this will get dealt
       * with in the memory setup by adjusting the source bitmap address.
       */
      uint32_t fractionalinc = ((qbinfo.verticalFilterInputSamples - 1) * m_fixedpointONE
                                / (qbinfo.verticalFilterOutputSamples ? (qbinfo.verticalFilterOutputSamples - 1):1));
      uint32_t fieldphase    = (((fractionalinc - m_fixedpointONE + (m_fixedpointONE>>8)))
                                / (m_fixedpointONE>>2)) % 8;

      if(qbinfo.InputFrameRect.y%2)
        pCaptureSetup->VSRC |= (fieldphase<<CAP_VSRC_TOP_INITIAL_PHASE_SHIFT);
      else
        pCaptureSetup->VSRC |= (fieldphase<<CAP_VSRC_BOT_INITIAL_PHASE_SHIFT);
    }

    /*
     * Do not enable V filtering in case of I2P capture as this would gives to
     * bad capture result.
     */
    if (!qbinfo.isBufferInterlaced)
    {
      /* Don't enable filtering for downsize up to 2 */
      if(qbinfo.vsrcinc >= (uint32_t)(2*m_fixedpointONE))
        goto vresize_program_ctl;
    }

    for(filter_index=0;filter_index<N_ELEMENTS(VSRC_index);filter_index++)
    {
      if(hw_srcinc <= VSRC_index[filter_index])
      {
        TRC( TRC_ID_UNCLASSIFIED, "  -> 3-Tap Polyphase Filtering (idx: %d)",  filter_index );

        pCaptureSetup->VFCn = (uint32_t )m_VFilter.pData + (filter_index*VFILTERS_ENTRY_SIZE);

        pCaptureSetup->VSRC |= CAP_VSRC_FILTER_EN;

        break;
      }
    }
  }

vresize_program_ctl:
  pCaptureSetup->CTL |= ulCtrl;

  return true;
}


void CGammaCompositorCAP::ConfigureCaptureWindowSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      GammaCaptureSetup_t                  *pCaptureSetup)
{
  uint32_t start_x, stop_x, start_y, stop_y;
  uint32_t uAVO, uAVS, uBCO, uBCS;

  /*
   * Set Capture viewport start, start on the first blanking line (negative offset
   * from the start of the active area).
   */
  start_x = qbinfo.InputFrameRect.x;

  int blankline = m_InputParams.vtotal - (qbinfo.InputVisibleArea.y - 1);
  if(qbinfo.isInputInterlaced)
    blankline *= 2;

  start_y = CaptureCalculateWindowLine(m_InputParams, blankline) + qbinfo.InputFrameRect.y;
  if(qbinfo.isInputInterlaced)
    start_y += qbinfo.InputFrameRect.y;

  uAVO = start_x | (start_y << 16);

  //Set Capture to start at the first active video line
  start_y = CaptureCalculateWindowLine(m_InputParams, 0);

  uBCO = start_x | (start_y << 16);

  /*
   * Set Active video stop, this is also where the capture stop as well
   */
  stop_x = start_x + qbinfo.InputFrameRect.width - 1;
  stop_y = start_y + qbinfo.InputHeight - 1;

  uAVS = stop_x | (stop_y << 16);
  uBCS = stop_x | (stop_y << 16);

  /* Use Video Active Area in case capturing video pilene */
  if((m_pCurrentInput.input_id == CAP_CTL_SOURCE_VID1) || (m_pCurrentInput.input_id == CAP_CTL_SOURCE_VID2))
  {
    /* FIXME : Check if this configuration is OK for all display modes */
    pCaptureSetup->CWO = uAVO;
    pCaptureSetup->CWS = uAVS;
  }
  else
  {
    pCaptureSetup->CWO = uBCO;
    pCaptureSetup->CWS = uBCS;
  }
}


void CGammaCompositorCAP::ConfigureCaptureBufferSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      GammaCaptureSetup_t                  *pCaptureSetup)
{
  uint32_t width       = qbinfo.BufferFrameRect.width;
  uint32_t top_height  = qbinfo.BufferHeight;
  uint32_t bot_height  = CAP_CMW_BOT_EQ_TOP_HEIGHT;

  if(qbinfo.isInputInterlaced && !qbinfo.isBufferInterlaced)
    top_height = top_height/2;

  pCaptureSetup->CMW = (bot_height | (top_height << 16) | width);
}


bool CGammaCompositorCAP::ConfigureCaptureColourFmt(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      GammaCaptureSetup_t                  *pCaptureSetup)
{
  uint32_t ulCtrl = 0;
  uint32_t buffer_address = 0;

  switch(qbinfo.BufferFormat.format)
  {
    case STM_PIXEL_FORMAT_RGB565:
      ulCtrl = CAP_CTL_RGB_565;
      buffer_address = pBuffer->rgb_address;
      break;

    case STM_PIXEL_FORMAT_RGB888:
      ulCtrl = CAP_CTL_RGB_888;
      buffer_address = pBuffer->rgb_address;
      break;

    case STM_PIXEL_FORMAT_ARGB1555:
      ulCtrl = CAP_CTL_ARGB_1555;
      buffer_address = pBuffer->rgb_address;
      break;

    case STM_PIXEL_FORMAT_ARGB8565:
      ulCtrl = CAP_CTL_ARGB_8565;
      buffer_address = pBuffer->rgb_address;
      break;

    case STM_PIXEL_FORMAT_ARGB8888:
      ulCtrl = CAP_CTL_ARGB_8888;
      buffer_address = pBuffer->rgb_address;
      break;

    case STM_PIXEL_FORMAT_ARGB4444:
      ulCtrl = CAP_CTL_ARGB_4444;
      buffer_address = pBuffer->rgb_address;
      break;

    case STM_PIXEL_FORMAT_YCbCr422R:
      ulCtrl = CAP_CTL_YCbCr422R;
      buffer_address = pBuffer->luma_address;
      break;

    case STM_PIXEL_FORMAT_YUV:
      ulCtrl = CAP_CTL_YCbCr888;
      buffer_address = pBuffer->luma_address;
      break;

    default:
      TRC( TRC_ID_ERROR, "Unknown colour format." );
      return false;
  }
  ulCtrl = (ulCtrl & CAP_CTL_FORMAT_MASK) << CAP_CTL_FORMAT_OFFSET;

  if((qbinfo.BufferFormat.color_space == STM_PIXEL_CAPTURE_BT709) ||
     (qbinfo.BufferFormat.color_space == STM_PIXEL_CAPTURE_BT709_FULLRANGE))
    ulCtrl |= CAP_CTL_BF_709_SELECT;

  pCaptureSetup->CTL |= ulCtrl;

  pCaptureSetup->VTP = buffer_address;
  if(qbinfo.isInputInterlaced)
    {
      pCaptureSetup->PMP = pBuffer->cap_format.stride * 2;
      pCaptureSetup->VBP = buffer_address + pBuffer->cap_format.stride;
    }
  else
    {
      pCaptureSetup->PMP = pBuffer->cap_format.stride;
      pCaptureSetup->VBP = buffer_address;
    }

  return true;
}


void CGammaCompositorCAP::ConfigureCaptureInput(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      GammaCaptureSetup_t                  *pCaptureSetup)
{
  uint32_t ulCtrl = 0;

  /*
   * The video inputs are begin captured in ARGB8888 format with
   * alpha 0..128 range.
   *
   * If the selected input is video1 or video2 then we should enable
   * RGB color space and disable YCbCr to RGB convertion.
   */
  if((m_pCurrentInput.input_id == CAP_CTL_SOURCE_VID1) || (m_pCurrentInput.input_id == CAP_CTL_SOURCE_VID2))
  {
    qbinfo.isInputRGB = true;
  }

  if(qbinfo.isInputRGB)
  {
    TRC( TRC_ID_UNCLASSIFIED, "RGB input selected" );
    ulCtrl  = ((m_pCurrentInput.input_id | CAPTURE_INPUT_CS_RGB) & CAP_CTL_SEL_SOURCE_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
  }
  else
  {
    TRC( TRC_ID_UNCLASSIFIED, "YCbCr input selected" );
    ulCtrl  = ((m_pCurrentInput.input_id) & CAP_CTL_SEL_SOURCE_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
  }
  ulCtrl |= (m_ulTimingID ? CAP_CTL_VTG_SELECT:0);

  if(((qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_RGB565) ||
      (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_RGB888) ||
      (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB1555) ||
      (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB4444) ||
      (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB8565) ||
      (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB8888)) &&
     (!qbinfo.isInputRGB))
  {
    ulCtrl |= CAP_CTL_YCBCR2RGB_ENA;
  }

  pCaptureSetup->CTL |= ulCtrl;
}


GammaCaptureSetup_t * CGammaCompositorCAP::PrepareCaptureSetup(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo)
{
  GammaCaptureSetup_t     *pCaptureSetup;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  // Allocate a buffer for Capture data. This buffer will be freed by CQueue::ReleaseNode
  pCaptureSetup = (GammaCaptureSetup_t *) vibe_os_allocate_memory (sizeof(GammaCaptureSetup_t));
  if(!pCaptureSetup)
  {
    TRC( TRC_ID_ERROR, "failed to allocate pCaptureSetup" );
    return 0;
  }
  vibe_os_zero_memory(pCaptureSetup, sizeof(GammaCaptureSetup_t));

  pCaptureSetup->buffer_address = (uint32_t)pBuffer;

  pCaptureSetup->CTL = (CAP_CTL_BFCAP_ENA | CAP_CTL_TFCAP_ENA);

  /* Start single shot capture for next VSync */
  /* If tunneled capture model, this flag will masked prior to submit */
  pCaptureSetup->CTL |= ( CAP_CTL_SSCAP_SINGLE_SHOT | CAP_CTL_CAPTURE_ENA);

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return pCaptureSetup;
}


bool CGammaCompositorCAP::PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer, CaptureQueueBufferInfo &qbinfo)
{
  capture_node_h              capNode   = {0};
  GammaCaptureSetup_t        *pCaptureSetup = (GammaCaptureSetup_t *)NULL;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  // Prepare capture
  pCaptureSetup = PrepareCaptureSetup(pBuffer, qbinfo);
  if(!pCaptureSetup)
  {
    TRC( TRC_ID_ERROR, "failed to allocate pCaptureSetup" );
    return false;
  }

  capNode = m_CaptureQueue->GetFreeNode();
  if(!capNode)
  {
    /* should wait until for a free node */
    vibe_os_free_memory(pCaptureSetup);
    return false;
  }

  if(!pBuffer->cap_format.stride)
  {
    vibe_os_free_memory(pCaptureSetup);
    return false;
  }

  AdjustBufferInfoForScaling(pBuffer, qbinfo);

  TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x hsrcinc = 0x%x chsrcinc = 0x%x",m_fixedpointONE, qbinfo.hsrcinc,qbinfo.chroma_hsrcinc );
  TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x vsrcinc = 0x%x cvsrcinc = 0x%x",m_fixedpointONE, qbinfo.vsrcinc,qbinfo.chroma_vsrcinc );

  /*
   * Configure the capture Colour Format.
   */
  if(!ConfigureCaptureColourFmt(pBuffer, qbinfo, pCaptureSetup))
  {
    TRC( TRC_ID_ERROR, "failed to configure capture buffer format" );
    vibe_os_free_memory(pCaptureSetup);
    return false;
  }

  /*
   * Configure the capture Resize and Filter.
   */
  if(!ConfigureCaptureResizeAndFilters(pBuffer, qbinfo, pCaptureSetup))
  {
    TRC( TRC_ID_ERROR, "failed to configure capture resize and filters" );
    return false;
  }

  /* Configure the capture source size */
  ConfigureCaptureWindowSize(pBuffer, qbinfo, pCaptureSetup);

  /* Configure the capture buffer (memory) size */
  ConfigureCaptureBufferSize(pBuffer, qbinfo, pCaptureSetup);

  /* Configure the capture input */
  ConfigureCaptureInput(pBuffer, qbinfo, pCaptureSetup);

  /* Set the new node data */
  m_CaptureQueue->SetNodeData(capNode,pCaptureSetup);

  pCaptureSetup = 0;  // This buffer is now under the control of queue_node

  /* Finally queue the new capture node */
  if(!m_CaptureQueue->QueueNode(capNode))
  {
    m_CaptureQueue->ReleaseNode(capNode);
    return false;
  }

  /*
   * Until now the capture is started and a buffer will need to be
   * release later.
   */
  m_hasBuffersToRelease = true;

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return true;
}


void CGammaCompositorCAP::Flush(bool bFlushCurrentNode)
{
  uint32_t          nodeReleasedNb = 0;
  capture_node_h    cut_node;           // The Queue will be cut just before this node
  capture_node_h    tail = 0;           // Remaining part of the queue after the cut

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if (bFlushCurrentNode)
  {
    TRC( TRC_ID_MAIN_INFO, "Capture Flush ALL nodes" );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Capture Flush nodes not on processing" );
  }

  ///////////////////////////////////////    Critical section      ///////////////////////////////////////////

  // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
  if ( vibe_os_down_semaphore(m_QueueLock) != 0 )
    return;

  if (bFlushCurrentNode)
  {
    // Flush ALL nodes

    // Cut the Queue at its head
    cut_node = m_CaptureQueue->GetNextNode(0);

    if (cut_node)
    {
      if (m_CaptureQueue->CutTail(cut_node))
      {
        tail = cut_node;
      }
    }

    // References to the previous, pending and current nodes are reset
    m_pendingNode  = 0;
    m_currentNode  = 0;

    /* nothing to release */
    m_hasBuffersToRelease = false;
  }
  else
  {
    // Flush the nodes not on processing

    // Cut the Queue after the pending node
    cut_node = m_CaptureQueue->GetNextNode(m_pendingNode);

    if (cut_node)
    {
      if (m_CaptureQueue->CutTail(cut_node))
      {
        tail = cut_node;
      }
    }
  }

  vibe_os_up_semaphore(m_QueueLock);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////

  // It is now safe to release all the nodes in "tail"

  while(tail != 0)
  {
    capture_node_h nodeToRelease = tail;

    // Get the next node BEFORE releasing the current node (otherwise we lose the reference)
    tail = m_CaptureQueue->GetNextNode(tail);

    // Flush pused buffer before releasing the node
    if(IsAttached() && m_PushGetInterface.push_buffer)
    {
      if(PushCapturedNode(nodeToRelease, 0, 0, true) == 0)
      {
        nodeReleasedNb++;
      }
    }
    else
    {
      RevokeCaptureBufferAccess(nodeToRelease);
      m_CaptureQueue->ReleaseNode(nodeToRelease);
      nodeReleasedNb++;
      m_Statistics.PicReleased++;
    }
  }

  if(nodeReleasedNb == m_NumberOfQueuedBuffer)
  {
    m_NumberOfQueuedBuffer = 0;
    m_hasBuffersToRelease = false;
  }

  TRC( TRC_ID_MAIN_INFO, "%d nodes released.", nodeReleasedNb );

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}


int CGammaCompositorCAP::ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  /*
   * Don't allow the release of queued nodes for tunneled capture mode.
   * This will be done by the driver when stopping the capture.
   */
  if(IsAttached())
  {
    TRC( TRC_ID_ERROR, "CGammaCompositorCAP::ReleaseBuffer 'operation not supported'" );
    return -ENOTSUPP;
  }

  if(m_hasBuffersToRelease && m_CaptureQueue)
  {
    capture_node_h       pNode = 0;
    GammaCaptureSetup_t *setup = (GammaCaptureSetup_t *)NULL;

    TRC( TRC_ID_UNCLASSIFIED, "capture = %u releasing previous node", GetID() );

    // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
    if (vibe_os_down_semaphore(m_QueueLock) != 0 )
      return -EINTR;

    pNode = m_CaptureQueue->GetNextNode(0);

    vibe_os_up_semaphore(m_QueueLock);

    while(pNode != 0)
    {
      /* Check if it is the requested node for release */
      setup = (GammaCaptureSetup_t *)m_CaptureQueue->GetNodeData(pNode);
      if(setup && setup->buffer_address == (uint32_t)pBuffer)
        break;
      pNode = m_CaptureQueue->GetNextNode(pNode);
    }

    if(!setup || !pNode) // nothing to release!
    {
      TRC( TRC_ID_ERROR, "CGammaCompositorCAP::ReleaseBuffer 'nothing to release?'" );
      return 0;
    }

    TRC( TRC_ID_UNCLASSIFIED, "CGammaCompositorCAP::ReleaseBuffer 'got node 0x%p for release'", pNode );

    /* Check if the node is already processed by the hw before releasing it */
    vibe_os_wait_queue_event(m_WaitQueueEvent, &setup->buffer_processed, 1, STMIOS_WAIT_COND_EQUAL, CAPTURE_RELEASE_WAIT_TIMEOUT*1000);
    TRC( TRC_ID_UNCLASSIFIED, "CGammaCompositorCAP::ReleaseBuffer 'releasing Node %p (buffer = 0x%x | processed? (%d) )'", pNode, setup->buffer_address, setup->buffer_processed );

    // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we release the node
    if (vibe_os_down_semaphore(m_QueueLock) != 0 )
      return -EINTR;

    m_CaptureQueue->ReleaseNode(pNode);
    m_NumberOfQueuedBuffer--;

    if(!m_NumberOfQueuedBuffer)
      m_hasBuffersToRelease = false;

    m_Statistics.PicReleased++;

    vibe_os_up_semaphore(m_QueueLock);
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  /*
   * We may have a suspend event in progress which did forcing the
   * release of this buffer.
   */
  return m_bIsSuspended ? (-EAGAIN) : 0;
}


int CGammaCompositorCAP::NotifyEvent(capture_node_h pNode, stm_pixel_capture_time_t vsyncTime)
{
  int res=0;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(pNode && m_CaptureQueue->IsValidNode(pNode))
  {
    GammaCaptureSetup_t *setup = (GammaCaptureSetup_t *)m_CaptureQueue->GetNodeData(pNode);
    if(setup)
    {
      /* Mark this buffer as processed */
      setup->buffer_processed = 1;

      /* Set back the new node data */
      m_CaptureQueue->SetNodeData(pNode,setup);

      m_Statistics.PicCaptured++;

      /* Notify captured buffer */
      res = CCapture::NotifyEvent(pNode, vsyncTime);

      TRC( TRC_ID_UNCLASSIFIED, "'Notify Node %p (buffer = 0x%x | processed? (%d) )'", pNode, setup->buffer_address,setup->buffer_processed );
    }
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return res;
}


void CGammaCompositorCAP::writeFieldSetup(const GammaCaptureSetup_t *setup, bool isTopField)
{
  uint32_t  *regval;
  uint32_t   i,Add;
  uint32_t   CTL;

  TRC( TRC_ID_UNCLASSIFIED, "%s field !", isTopField ? "TOP" : "BOT" );

  WriteCaptureReg(GAM_CAPn_CWO , setup->CWO);
  WriteCaptureReg(GAM_CAPn_CWS , setup->CWS);
  WriteCaptureReg(GAM_CAPn_VTP , setup->VTP);
  WriteCaptureReg(GAM_CAPn_VBP , setup->VBP);
  WriteCaptureReg(GAM_CAPn_PMP , setup->PMP);
  WriteCaptureReg(GAM_CAPn_CMW , setup->CMW);

  if (setup->CTL & CAP_CTL_EN_H_RESIZE)
  {
    WriteCaptureReg(GAM_CAPn_HSRC , setup->HSRC);

    /* Set horizontal filter coeffs */
    if (setup->HSRC & CAP_HSRC_FILTER_EN)
    {
      Add = GAM_CAPn_HFC0;
      regval = (uint32_t *)setup->HFCn;
      if(regval)
      {
        for( i=0; i<(NB_HSRC_COEFFS/4); i++, Add+=4, regval+=1)
        {
          TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_HFC%d \t = 0x%08x'", i, *regval );
          WriteCaptureReg(Add,*regval);
        }
      }
    }
  }

  if (setup->CTL & CAP_CTL_EN_V_RESIZE)
  {
    WriteCaptureReg(GAM_CAPn_VSRC , setup->VSRC);

    /* Set vertical filter coeffs */
    if (setup->VSRC & CAP_VSRC_FILTER_EN)
    {
      Add = GAM_CAPn_VFC0;
      regval = (uint32_t *)setup->VFCn;
      if(regval)
      {
        for( i=0; i<(NB_VSRC_COEFFS/4); i++, Add+=4, regval+=1)
        {
          TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_VFC%d \t = 0x%08x'", i, *regval );
          WriteCaptureReg(Add,*regval);
        }
      }
    }
  }

  CTL = (m_Sink)?(setup->CTL&~CAP_CTL_SSCAP_SINGLE_SHOT):setup->CTL;

  /*
   * For Tunneled compo capture we update CTL register only if we have
   * changed configuration.
   *
   * For Single Shot capture mode we need to always program the CTL register.
   */
  if(IsAttached())
  {
    if(CTL != m_ulCaptureCTL)
    {
      m_ulCaptureCTL = CTL;
      WriteCaptureReg(GAM_CAPn_CTL , m_ulCaptureCTL);
    }
  }
  else
  {
    WriteCaptureReg(GAM_CAPn_CTL , CTL);
  }

  m_Statistics.PicInjected++;

#if defined(CAPTURE_DEBUG_REGISTERS)
  if(ctx == 1000)
  {
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_CTL \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_CTL) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_CWO \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_CWO) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_CWS \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_CWS) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_VTP \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_VTP) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_VBP \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_VBP) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_PMP \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_PMP) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_CMW \t = 0x%08x'",  ReadCaptureReg(GAM_CAPn_CMW) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_HSRC \t = 0x%08x'", ReadCaptureReg(GAM_CAPn_HSRC) );
    TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_VSRC \t = 0x%08x'", ReadCaptureReg(GAM_CAPn_VSRC) );

    for(i=0; i < NB_HSRC_COEFFS/4; i++)
      TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_HFC%d \t = 0x%08x'", i, ReadCaptureReg((GAM_CAPn_HFC0 + (4*i))) );

    for(i=0; i < NB_VSRC_COEFFS/4; i++)
      TRC( TRC_ID_UNCLASSIFIED, "'GAM_CAPn_VFC%d \t = 0x%08x'", i, ReadCaptureReg((GAM_CAPn_VFC0 + (4*i))) );

    ctx = 0;
  }
  ctx++;
#endif
}


int CGammaCompositorCAP::Start(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_isStarted)
  {
    /* Do not start hw right now!! */
    CCapture::Start();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


int CGammaCompositorCAP::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_isStarted)
  {
    uint32_t ulCtrl = ReadCaptureReg(GAM_CAPn_CTL);
    ulCtrl &= ~CAP_CTL_CAPTURE_ENA;
    WriteCaptureReg(GAM_CAPn_CTL , ulCtrl);

    /* Invalidate previous CTL value */
    m_ulCaptureCTL = CAPTURE_HW_INVALID_CTL;

    CCapture::Stop();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


void CGammaCompositorCAP::ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  capture_node_h       nodeCandidateForCapture = 0;
  bool                 isTopField=true;
  bool                 isInputInterlaced=false;

  // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
  if (vibe_os_down_semaphore(m_QueueLock) != 0 )
    goto nothing_to_process_in_capture;

  isTopField        = !!(timingevent & STM_TIMING_EVENT_TOP_FIELD);
  isInputInterlaced = !!((m_InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);

  TRC( TRC_ID_UNCLASSIFIED, "isTopField = %d  | isInputInterlaced = %d", (int)isTopField, (int)isInputInterlaced);

  if(!m_isStarted)
  {
    /* Do not program Capture Configuration */
    goto nothing_to_process_in_capture;
  }

  /*
   * Invalidate Nodes when VSync signal occurs.
   */
  m_Statistics.PicVSyncNb++;

  if (m_previousNode && (!m_CaptureQueue->IsValidNode(m_previousNode)) )
  {
    TRC( TRC_ID_UNCLASSIFIED, "Error invalid m_previousNode 0x%p!",m_previousNode );
    m_previousNode = 0;
  }

  if (m_currentNode && (!m_CaptureQueue->IsValidNode(m_currentNode)) )
  {
    TRC( TRC_ID_UNCLASSIFIED, "Error invalid m_currentNode 0x%p!",m_currentNode );
    m_currentNode = 0;
  }

  if (m_pendingNode && (!m_CaptureQueue->IsValidNode(m_pendingNode)) )
  {
    TRC( TRC_ID_UNCLASSIFIED, "Error invalid m_pendingNode 0x%p!",m_pendingNode);
    m_pendingNode = 0;
  }

  /*
   * Make sure to push back all remainig nodes that were queued before the
   * actual marked previous node.
   */
  if(!isTopField || !isInputInterlaced)
  {
    if((m_previousNode) && m_CaptureQueue->IsValidNode(m_previousNode))
    {
      capture_node_h first_node = m_CaptureQueue->GetNextNode(0);
      while ((first_node)
          && (first_node != m_currentNode)
          && (m_CaptureQueue->IsValidNode(first_node)))
      {
        if(PushCapturedNode(first_node, vsyncTime, 0, true) < 0)
        {
          TRC( TRC_ID_ERROR, "Push Captured Node failed!!!" );
        }
        first_node = m_CaptureQueue->GetNextNode(0);
        m_Statistics.PicSkipped++;
      }
      m_previousNode = 0;
    }
  }

  // A new TOP VSync has happen so:
  //  - the PendingNode is going to become the CurrentNode
  //  - the CurrentNode is going to be released (if the pending picture is a new one)
  //  - a new node is going to be prepared for next VSync and will become the PendingNode
  CCapture::UpdateCurrentNode(vsyncTime);

  /* If the input is interlaced then we update nodes values only during the BOTTOM field */
  if(!isTopField || !isInputInterlaced)
  {
    // The current node become the previous node so it will notified later
    m_previousNode = m_currentNode;

    // The pending node become the current node
    m_currentNode  = m_pendingNode;

    // The pending node become the current node
    m_pendingNode = 0;

    /*
     * If the input is interlaced then we release the buffer only during the TOP
     * field as this event is always coming during the current field.
     */
    m_Statistics.PicCaptured++;

    if((m_previousNode) && m_CaptureQueue->IsValidNode(m_previousNode))
    {
      // Push back the previous node if it was not yet done during capture
      // event.
      if(PushCapturedNode(m_previousNode, vsyncTime, 1, true) < 0)
      {
        /* Skip this event */
        TRC( TRC_ID_ERROR, "Push Captured Node failed!!!" );
        goto skip_capture_event;
      }
      m_previousNode = 0;
    }
    else
    {
      TRC( TRC_ID_UNCLASSIFIED, "Invalid previous node !!!" );
    }

    // Now find a candidate for next VSync
    if (m_currentNode == 0)
    {
      // No picture was currently captured: Get the first picture of the queue
      nodeCandidateForCapture = m_CaptureQueue->GetNextNode(0);

      if (nodeCandidateForCapture)
        TRC( TRC_ID_UNCLASSIFIED, "No buffer currently queued. Use the 1st node of the queue: %p", nodeCandidateForCapture );
    }
    else
    {
      // Get the next buffer in the queue
      nodeCandidateForCapture = m_CaptureQueue->GetNextNode(m_currentNode);

      if (nodeCandidateForCapture)
        TRC( TRC_ID_UNCLASSIFIED, "Move to next node: %p", nodeCandidateForCapture );
    }

    if (!nodeCandidateForCapture)
    {
      if (GetCaptureNode(nodeCandidateForCapture, true) < 0)
      {
        /* Skip this event */
        TRC( TRC_ID_ERROR, "Get Capture Node failed!!!" );
        goto skip_capture_event;
      }
    }
  }
  else
  {
    if (m_pendingNode)
    {
      nodeCandidateForCapture = m_pendingNode;
      TRC( TRC_ID_UNCLASSIFIED, "Keep using current node %p for BOTTOM field capture", nodeCandidateForCapture );
    }
  }

  if((nodeCandidateForCapture) && m_CaptureQueue->IsValidNode(nodeCandidateForCapture))
  {
    stm_i_push_get_sink_get_desc_t   *pGetBufferDesc = (stm_i_push_get_sink_get_desc_t *)m_CaptureQueue->GetNodeData(nodeCandidateForCapture);
    stm_pixel_capture_buffer_descr_t  CaptureBuffer = { 0 };
    CaptureQueueBufferInfo            qbinfo        = { 0 };
    GammaCaptureSetup_t              *pCaptureSetup = (GammaCaptureSetup_t *)NULL;

    if(pGetBufferDesc)
    {
      TRC( TRC_ID_UNCLASSIFIED, "'Capture will be processing Node (%p) and Buffer (%p) for the new VSync'", nodeCandidateForCapture, (void *)pGetBufferDesc);

      if(pGetBufferDesc->video_buffer_addr == 0)
      {
        TRC( TRC_ID_UNCLASSIFIED, "Got Node with invalid buffer address (%p)\n", m_Sink);
        /* Release the node */
        m_CaptureQueue->ReleaseNode(nodeCandidateForCapture);
        m_Statistics.PicReleased++;

        if (GetCaptureNode(nodeCandidateForCapture, true) < 0)
        {
          TRC( TRC_ID_ERROR, "Error in GetCapturedNode(%p)\n", m_Sink);
          // Nothing to process! Skip this event.
          goto skip_capture_event;
        }
        pGetBufferDesc = (stm_i_push_get_sink_get_desc_t *)m_CaptureQueue->GetNodeData(nodeCandidateForCapture);
      }

      /*
       * Setup the capture buffer format and descriptor according to new push
       * buffer descriptor data.
       */
      if(pGetBufferDesc)
      {
        CaptureBuffer.cap_format     = m_CaptureFormat;
        CaptureBuffer.bytesused      = pGetBufferDesc->height * pGetBufferDesc->pitch;
        CaptureBuffer.length         = pGetBufferDesc->height * pGetBufferDesc->pitch;
        if((CaptureBuffer.cap_format.format == STM_PIXEL_FORMAT_RGB_8B8B8B_SP) ||
           (CaptureBuffer.cap_format.format == STM_PIXEL_FORMAT_RGB_10B10B10B_SP))
        {
          CaptureBuffer.rgb_address   = pGetBufferDesc->video_buffer_addr;
        }
        else
        {
          CaptureBuffer.luma_address   = pGetBufferDesc->video_buffer_addr;
          CaptureBuffer.chroma_offset  = pGetBufferDesc->chroma_buffer_offset;
        }
        CaptureBuffer.captured_time  = vsyncTime + m_FrameDuration;

        if(!GetQueueBufferInfo(&CaptureBuffer, qbinfo))
        {
          TRC( TRC_ID_ERROR, "failed to get new buffer queue info data");
          goto skip_capture_event;
        }

        AdjustBufferInfoForScaling(&CaptureBuffer, qbinfo);

        TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x hsrcinc = 0x%x chsrcinc = 0x%x",m_fixedpointONE, qbinfo.hsrcinc,qbinfo.chroma_hsrcinc );
        TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x vsrcinc = 0x%x cvsrcinc = 0x%x",m_fixedpointONE, qbinfo.vsrcinc,qbinfo.chroma_vsrcinc );

        // Prepare capture
        pCaptureSetup = PrepareCaptureSetup(&CaptureBuffer, qbinfo);
        if(!pCaptureSetup)
        {
          TRC( TRC_ID_ERROR, "failed to allocate pCaptureSetup" );
          goto skip_capture_event;
        }

        /*
         * Configure the capture Colour Format.
         */
        if(!ConfigureCaptureColourFmt(&CaptureBuffer, qbinfo, pCaptureSetup))
        {
          TRC( TRC_ID_ERROR, "failed to configure capture buffer format" );
          vibe_os_free_memory(pCaptureSetup);
          m_CaptureQueue->ReleaseNode(nodeCandidateForCapture);
          goto skip_capture_event;
        }

        /*
         * Configure the capture Resize and Filter.
         */
        if(!ConfigureCaptureResizeAndFilters(&CaptureBuffer, qbinfo, pCaptureSetup))
        {
          TRC( TRC_ID_ERROR, "failed to configure capture resize and filters" );
          vibe_os_free_memory(pCaptureSetup);
          m_CaptureQueue->ReleaseNode(nodeCandidateForCapture);
          goto skip_capture_event;
        }

        /* Configure the capture source size */
        ConfigureCaptureWindowSize(&CaptureBuffer, qbinfo, pCaptureSetup);

        /* Configure the capture buffer (memory) size */
        ConfigureCaptureBufferSize(&CaptureBuffer, qbinfo, pCaptureSetup);

        /* Configure the capture input */
        ConfigureCaptureInput(&CaptureBuffer, qbinfo, pCaptureSetup);

        /* Write capture cfg */
        writeFieldSetup(pCaptureSetup, (!isTopField || !isInputInterlaced) ? true:false);

        /* Setup is no more needed */
        vibe_os_free_memory(pCaptureSetup);

        if(!isTopField || !isInputInterlaced)
        {
          SetPendingNode(nodeCandidateForCapture);
        }
      }
    }
  }

  vibe_os_up_semaphore(m_QueueLock);

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  return;

skip_capture_event:
  m_Statistics.PicSkipped++;
  vibe_os_up_semaphore(m_QueueLock);

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  return;

nothing_to_process_in_capture:
  {
    // The Capture is disabled to avoid writing an invalid buffer
    uint32_t ulCtrl = ReadCaptureReg(GAM_CAPn_CTL);
    ulCtrl &= ~CAP_CTL_CAPTURE_ENA;
    WriteCaptureReg(GAM_CAPn_CTL , ulCtrl);

    /* Invalidate previous CTL value */
    m_ulCaptureCTL = CAPTURE_HW_INVALID_CTL;
  }

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  vibe_os_up_semaphore(m_QueueLock);
}


void CGammaCompositorCAP::CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  capture_node_h       nodeCandidateForCapture = 0;
  GammaCaptureSetup_t *setup                   = (GammaCaptureSetup_t *)NULL;

  TRC( TRC_ID_UNCLASSIFIED, "CGammaCompositorCAP::CaptureUpdateHW 'vsyncTime = %llu'", vsyncTime );
  m_Statistics.PicVSyncNb++;

  // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
  if ( vibe_os_down_semaphore(m_QueueLock) != 0 )
    return;

  if ((!m_CaptureQueue) || (!m_isStarted))
  {
    // The queue is not created yet
    goto nothing_to_inject_in_capture;
  }

  // Check node validity
  if (m_previousNode && (!m_CaptureQueue->IsValidNode(m_previousNode)) )
  {
    TRC( TRC_ID_ERROR, "Error invalid m_previousNode 0x%p!",m_previousNode );
    m_previousNode = 0;
  }

  // Check node validity
  if (m_currentNode && (!m_CaptureQueue->IsValidNode(m_currentNode)) )
  {
    TRC( TRC_ID_ERROR, "Error invalid m_currentNode 0x%p!",m_currentNode );
    m_currentNode = 0;
  }

  if (m_pendingNode && (!m_CaptureQueue->IsValidNode(m_pendingNode)) )
  {
    TRC( TRC_ID_ERROR, "Error invalid m_pendingNode 0x%p!",m_pendingNode );
    m_pendingNode = 0;
  }

  if (m_bIsSuspended)
  {
    /* For non tunneled capture ... notify pending node before existing */
    if(m_currentNode != 0)
    {
      NotifyEvent(m_currentNode, vsyncTime);
      m_currentNode = 0;
    }
    goto nothing_to_inject_in_capture;
  }

  // A new VSync has happen so:
  //  - the pendingNode is going to become the currentNode
  //  - the currentNode is going to be notfied within next VSync
  //  - a new node is going to be prepared for next VSync and will become the pendingNode

  CCapture::UpdateCurrentNode(vsyncTime);

  // The current node become the previous node so it will notified later
  m_previousNode = m_currentNode;

  // The pending node become the current node
  m_currentNode  = m_pendingNode;
  m_pendingNode  = 0;

  if(m_previousNode != 0)
  {
    NotifyEvent(m_previousNode, vsyncTime);
    m_previousNode = 0;
  }

  // Now find a candidate for next VSync
  if (m_currentNode == 0)
  {
    // No picture was currently captured: Get the first picture of the queue
    nodeCandidateForCapture = m_CaptureQueue->GetNextNode(0);

    if (nodeCandidateForCapture)
      TRC( TRC_ID_UNCLASSIFIED, "No buffer currently queued. Use the 1st node of the queue: %p", nodeCandidateForCapture );
  }
  else
  {
    // Get the next buffer in the queue
    nodeCandidateForCapture = m_CaptureQueue->GetNextNode(m_currentNode);

    if (nodeCandidateForCapture)
      TRC( TRC_ID_UNCLASSIFIED, "Move to next node: %p", nodeCandidateForCapture );
  }

  if (!nodeCandidateForCapture)
  {
    // No valid candidate was found. If we had a currentNode, continue to process it
    if (m_currentNode)
    {
      nodeCandidateForCapture = m_currentNode;
      m_Statistics.PicRepeated++;
      TRC( TRC_ID_UNCLASSIFIED, "No valid candidate found. Keep processing node %p", nodeCandidateForCapture );
    }
    else
    {
      // Nothing to process!
      m_Statistics.PicSkipped++;
      goto nothing_to_inject_in_capture;
    }
  }

  if(nodeCandidateForCapture)
  {
    setup = (GammaCaptureSetup_t *)m_CaptureQueue->GetNodeData(nodeCandidateForCapture);
    if(setup)
      writeFieldSetup(setup, true);

    SetPendingNode(nodeCandidateForCapture);
  }

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  vibe_os_up_semaphore(m_QueueLock);
  return;

nothing_to_inject_in_capture:
  {
    // The Capture is disabled to avoid writing an invalid buffer
    uint32_t ulCtrl = ReadCaptureReg(GAM_CAPn_CTL);
    ulCtrl &= ~CAP_CTL_CAPTURE_ENA;
    WriteCaptureReg(GAM_CAPn_CTL , ulCtrl);

    /* Invalidate previous CTL value */
    m_ulCaptureCTL = CAPTURE_HW_INVALID_CTL;
  }

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  vibe_os_up_semaphore(m_QueueLock);
}


void CGammaCompositorCAP::RevokeCaptureBufferAccess(capture_node_h pNode)
{
  TRC( TRC_ID_MAIN_INFO, "revoking .");
  if(pNode && m_CaptureQueue->IsValidNode(pNode))
  {
    if(!IsAttached())
    {
      GammaCaptureSetup_t *setup = (GammaCaptureSetup_t *)m_CaptureQueue->GetNodeData(pNode);
      stm_pixel_capture_buffer_descr_t *bufferToRelease;

      if(setup)
      {
        bufferToRelease = (stm_pixel_capture_buffer_descr_t *)setup->buffer_address;
        CCapture::RevokeCaptureBufferAccess(bufferToRelease);
        TRC( TRC_ID_MAIN_INFO, "done revoking.");
      }
    }
  }
}
