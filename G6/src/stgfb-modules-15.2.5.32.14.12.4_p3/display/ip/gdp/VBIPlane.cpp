/***********************************************************************
 *
 * File: display/ip/gdp/VBIPlane.cpp
 * Copyright (c) 2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>

#include "display/ip/gdp/GdpReg.h"
#include "display/ip/gdp/GdpDefs.h"

#include "VBIPlane.h"

CVBIPlane::CVBIPlane(
    const char          *name,
    uint32_t             id,
    const stm_plane_capabilities_t caps,
    CGdpPlane *linktogdp): CGdpPlane(name,
                                          id,
                                          caps,
                                          linktogdp)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Override the base class initialization so the Create method will link
   * us to the head of the node list currently represented by the GDP we
   * been passed in "linktogdp".
   */
  m_NextGDPNodeOwner = linktogdp;

  /*
   * Do not allow vertical scaling in the VBI.
   */
  m_ulMaxVSrcInc   = m_fixedpointONE;
  m_ulMinVSrcInc   = m_fixedpointONE;

  /* Set default VBI Plane Type A */
  m_bIsTypeB = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CVBIPlane::GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  range->min_val.x      = -28;
  range->min_val.y      =  -6;
  range->max_val.x      =   0;
  range->max_val.y      =  -2;

  range->min_val.height =   1;
  range->min_val.width  =   (720 - range->max_val.x);
  range->max_val.height =   2;
  range->max_val.width  =   ((1920 - range->min_val.x) * 2);

  range->default_val.x = 0;
  range->default_val.y = 0;
  range->default_val.width = 0;
  range->default_val.height = 0;

  range->step.x = 1;
  range->step.y = 1;
  range->step.width = 1;
  range->step.height = 1;

  if(m_pOutput)
  {
    const stm_display_mode_t* pCurrentMode = m_pOutput->GetCurrentDisplayMode();

    if(pCurrentMode)
    {
      /*
       * Override the maximum height from the video area to the VBI area.
       */
      range->min_val.x      = pCurrentMode->mode_timing.hsync_width - pCurrentMode->mode_params.active_area_start_pixel;
      range->min_val.y      = pCurrentMode->mode_timing.vsync_width - (pCurrentMode->mode_params.active_area_start_line-1);
      range->min_val.height = 1;
      range->min_val.width  = 1;

      if((pCurrentMode->mode_id == STM_TIMING_MODE_720P59940_74176) || (pCurrentMode->mode_id == STM_TIMING_MODE_720P60000_74250))
        range->max_val.x    =-28;
      else
        range->max_val.x    = 0;

      if(m_bIsTypeB)
        range->max_val.y    =-3;
      else
        range->max_val.y    = -2;
      range->max_val.width  = pCurrentMode->mode_params.active_area_width - range->max_val.x;
      range->max_val.height = 1;

      if(pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN)
      {
        range->min_val.y      *= 2;
        range->min_val.height *= 2;
        range->max_val.y      *= 2;
        range->max_val.height *= 2;
      }
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CVBIPlane::GetScalingContext(const InputFrameInfo &frame, ScalingContext &scalingContext) const
{
  const stm_display_mode_t* pCurrentMode;

  scalingContext.SrcVisibleArea      = frame.SrcVisibleArea;
  scalingContext.IsSrcInterlaced     = ((frame.SrcFlags & STM_BUFFER_SRC_INTERLACED) == STM_BUFFER_SRC_INTERLACED);

  if( (frame.SrcPixelAspectRatio.numerator   > 1) ||
      (frame.SrcPixelAspectRatio.denominator > 1) )
  {
    TRC( TRC_ID_ERROR, "Invalid source aspect ratio!" );
    return false;
  }

  scalingContext.SrcPixelAspectRatio.numerator   = 1;
  scalingContext.SrcPixelAspectRatio.denominator = 1;

  if (!m_pOutput)
  {
    TRC( TRC_ID_ERROR, "Plane not connected to any output. Impossible to compute the scalingContext!" );
    return false;
  }

  pCurrentMode= m_pOutput->GetCurrentDisplayMode();
  if(!pCurrentMode)
  {
    TRC( TRC_ID_ERROR, "Impossible to get the current display mode. Impossible to compute the scalingContext!" );
    return false;
  }

  scalingContext.DisplayPixelAspectRatio.numerator   = 1;
  scalingContext.DisplayPixelAspectRatio.denominator = 1;

  scalingContext.IsDisplayInterlaced = (pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN);

  if((pCurrentMode->mode_id == STM_TIMING_MODE_720P59940_74176) || (pCurrentMode->mode_id == STM_TIMING_MODE_720P60000_74250))
    scalingContext.DisplayVisibleArea.x      =-28;
  else
    scalingContext.DisplayVisibleArea.x      =  0;

  if(m_bIsTypeB)
    scalingContext.DisplayVisibleArea.y      = -3;
  else
    scalingContext.DisplayVisibleArea.y      = -2;

  scalingContext.DisplayVisibleArea.width  = ( pCurrentMode->mode_params.active_area_width
                                             - scalingContext.DisplayVisibleArea.x );

  scalingContext.DisplayVisibleArea.height = 1;

  if(scalingContext.IsDisplayInterlaced)
  {
    scalingContext.DisplayVisibleArea.y      *= 2;
    scalingContext.DisplayVisibleArea.height *= 2;
  }

  TRC( TRC_ID_VBI_PLANE, "Input Window  : (%d,%d) - (%dx%d)",
          scalingContext.SrcVisibleArea.x,
          scalingContext.SrcVisibleArea.y,
          scalingContext.SrcVisibleArea.width,
          scalingContext.SrcVisibleArea.height );

  TRC( TRC_ID_VBI_PLANE, "Output Window : (%d,%d) - (%dx%d)",
          scalingContext.DisplayVisibleArea.x,
          scalingContext.DisplayVisibleArea.y,
          scalingContext.DisplayVisibleArea.width,
          scalingContext.DisplayVisibleArea.height );

  return true;
}


bool CVBIPlane::PrepareIOWindows(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
  InputFrameInfo frame;
  ScalingContext scalingContext;

  frame.SrcVisibleArea      = pNodeToDisplay->m_bufferDesc.src.visible_area;
  frame.SrcPixelAspectRatio = pNodeToDisplay->m_bufferDesc.src.pixel_aspect_ratio;
  frame.SrcFlags            = pNodeToDisplay->m_bufferDesc.src.flags;
  frame.Src3DFormat         = pNodeToDisplay->m_bufferDesc.src.config_3D.formats;

  FillSelectedPictureDisplayInfo(pNodeToDisplay, pDisplayInfo);

  if (!GetScalingContext(frame, scalingContext))
    return false;

  if((scalingContext.IsSrcInterlaced != scalingContext.IsDisplayInterlaced) ||
     (scalingContext.SrcVisibleArea.width != scalingContext.DisplayVisibleArea.width) ||
     (scalingContext.SrcVisibleArea.height != scalingContext.DisplayVisibleArea.height) )
  {
    TRC( TRC_ID_ERROR, "Input and Output rectangles should be the same!!" );
    return false;
  }

  // Save computed input and output window value to return to user in AUTO mode
  m_ComputedInputWindowValue  = scalingContext.SrcVisibleArea;
  m_ComputedOutputWindowValue = scalingContext.DisplayVisibleArea;

  pDisplayInfo->m_selectedPicture.srcFrameRect = scalingContext.SrcVisibleArea;

  pDisplayInfo->m_selectedPicture.srcHeight = (pDisplayInfo->m_isSrcInterlaced) ?
                                               pDisplayInfo->m_selectedPicture.srcFrameRect.height / 2 :
                                               pDisplayInfo->m_selectedPicture.srcFrameRect.height;

  pDisplayInfo->m_dstFrameRect = scalingContext.DisplayVisibleArea;

  if(scalingContext.IsDisplayInterlaced)
  {
      // Interlaced output

      /*
       * Note that:
       * - the start line must also be located on a top field to prevent fields
       *   getting accidentally swapped so "y" must be even.
       * - we do not support an "odd" number of lines on an interlaced display,
       *   there must always be a matched top and bottom field line.
       */
      pDisplayInfo->m_dstFrameRect.y = pDisplayInfo->m_dstFrameRect.y & ~(0x1);
      pDisplayInfo->m_dstHeight      = pDisplayInfo->m_dstFrameRect.height / 2;
  }
  else
  {
      // Progressive output
      pDisplayInfo->m_dstHeight      = pDisplayInfo->m_dstFrameRect.height;
  }

  pDisplayInfo->m_areIOWindowsValid = true;

  return true;
}


bool CVBIPlane::setOutputViewport(GENERIC_GDP_LLU_NODE          &topNode,
                                  GENERIC_GDP_LLU_NODE          &botNode)
{
  if(!CGdpPlane::setOutputViewport(topNode,botNode))
    return false;

  topNode.GDPn_PPT |= (GDP_PPT_FORCE_ON_MIX1 |
                       GDP_PPT_FORCE_ON_MIX2);

  botNode.GDPn_PPT |= (GDP_PPT_FORCE_ON_MIX1 |
                       GDP_PPT_FORCE_ON_MIX2);

  return true;
}


void CVBIPlane::createDummyNode(GENERIC_GDP_LLU_NODE *node)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  CGdpPlane::createDummyNode(node);
  /*
   * Override the default viewport to be in the VBI region
   */
  node->GDPn_SIZE = 0x00010001;
  node->GDPn_VPO  = 0x00070010;
  node->GDPn_VPS  = 0x00070010;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CVBIPlane::PresentDisplayNode(CDisplayNode *pPrevNode,
                                    CDisplayNode *pCurrNode,
                                    CDisplayNode *pNextNode,
                                    bool isPictureRepeated,
                                    bool isInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const stm_time64_t &vsyncTime)
{
  if(!pCurrNode)
  {
    /*Nothing to display*/
    return;
  }

  /*
   * The VBI plane is not able to perform any scaling so:
   * The Input and Output windows should be in Auto mode
   * and
   * The Input and Output rectangles should be equal
   */
  if ((m_InputWindowMode == MANUAL_MODE) || (m_OutputWindowMode == MANUAL_MODE) )
  {
    bool IsSrcInterlaced = ((pCurrNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED) == STM_BUFFER_SRC_INTERLACED);
    int TypeA_Ymin = (IsSrcInterlaced ? -4 : -2);
    if(m_OutputWindowValue.y < TypeA_Ymin)
      m_bIsTypeB = true;
    else
      m_bIsTypeB = false;
  }

  CGdpPlane::PresentDisplayNode(pPrevNode,
                                pCurrNode,
                                pNextNode,
                                isPictureRepeated,
                                isInterlaced,
                                isTopFieldOnDisplay,
                                vsyncTime);
}

