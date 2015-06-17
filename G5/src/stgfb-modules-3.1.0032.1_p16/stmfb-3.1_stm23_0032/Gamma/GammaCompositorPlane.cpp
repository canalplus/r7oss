/***********************************************************************
 *
 * File: Gamma/GammaCompositorPlane.cpp
 * Copyright (c) 2005 by STMicroelectronics. All rights reserved.
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

#include <STMCommon/stmviewport.h>
#include <STMCommon/stmfilter.h>

#include "GammaCompositorPlane.h"

/*
 * This define sets the debug message level used for all non-linear-zoom error
 * paths that result in us falling back to a linear zoom.
 */
#define DEBUG_NLZ_LEVEL 4

///////////////////////////////////////////////////////////////////////////////
// Base class for compositor hardware planes, GDP, VidPLUG, Cursor and ALPHA
//
CGammaCompositorPlane::CGammaCompositorPlane(stm_plane_id_t planeID): CDisplayPlane(planeID)
{
    m_pSurfaceFormats = 0;
    m_nFormats        = 0;

    m_bHasNonLinearZoom            = false;
    m_bUsePiagetAppNoteEquations   = false;
    m_bHasProgressive2InterlacedHW = false;
}


CGammaCompositorPlane::~CGammaCompositorPlane() {}


int CGammaCompositorPlane::GetFormats(const SURF_FMT** pData) const
{
    *pData = m_pSurfaceFormats;
    return m_nFormats;
}


/////////////////////////////////////////////////////////////////
// Helper function for plane QueueBuffer implementations that
// extract flags and do some common processing for interlaced
// displays and content.
bool CGammaCompositorPlane::GetQueueBufferInfo(const stm_display_buffer_t * const pFrame,
                                               const void                 * const user,
                                               GAMMA_QUEUE_BUFFER_INFO    &qbinfo) const
{
    /*
     * Don't allow buffers to be queued if the plane is not connected
     * to an output or that output doesn't have a valid display mode initialised
     */
    if(!m_pOutput)
        return false;

    if(m_user != user)
    {
        DEBUGF2(3,("%s: mis-matched user handle\n", __PRETTY_FUNCTION__));
        return false;
    }

    g_pIOS->ZeroMemory(&qbinfo,sizeof(qbinfo));

    qbinfo.pCurrentMode = m_pOutput->GetCurrentDisplayMode();

    if(!qbinfo.pCurrentMode)
        return false;

    qbinfo.isDisplayInterlaced = (qbinfo.pCurrentMode->ModeParams.ScanType == SCAN_I);
    qbinfo.isSourceInterlaced  = ((pFrame->src.ulFlags & STM_PLANE_SRC_INTERLACED) == STM_PLANE_SRC_INTERLACED);

    if(qbinfo.isSourceInterlaced && !qbinfo.isDisplayInterlaced && !(m_capabilities.ulCaps & PLANE_CAPS_DEINTERLACE))
    {
        DEBUGF2(3,("%s: plane doesn't support de-interlacing\n", __PRETTY_FUNCTION__));
        return false;
    }

    if(pFrame->src.ulColorFmt == SURF_YCBCR422MB ||
       pFrame->src.ulColorFmt == SURF_YCBCR420MB)
    {
        qbinfo.isSrcMacroBlock = true;
    }

    switch(pFrame->src.ulColorFmt)
    {
        case SURF_YCBCR420MB:
        case SURF_YUV420:
        case SURF_YVU420:
            qbinfo.isSrc420 = true;
            break;

        case SURF_YCBCR422MB:
        case SURF_YCBCR422R:
        case SURF_YUYV:
        case SURF_YUV422P:
            qbinfo.isSrc422 = true;
            break;

        default:
            break;
    }

    if(qbinfo.isSourceInterlaced)
    {
        if((pFrame->src.ulFlags & (STM_PLANE_SRC_TOP_FIELD_ONLY|STM_PLANE_SRC_BOTTOM_FIELD_ONLY)) != 0)
        {
            qbinfo.firstFieldType   = ((pFrame->src.ulFlags & STM_PLANE_SRC_TOP_FIELD_ONLY) != 0)?GNODE_TOP_FIELD:GNODE_BOTTOM_FIELD;
            qbinfo.firstFieldOnly   = true;
        }
        else
        {
            qbinfo.firstFieldType   = ((pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST) != 0)?GNODE_BOTTOM_FIELD:GNODE_TOP_FIELD;
            qbinfo.repeatFirstField = ((pFrame->src.ulFlags & STM_PLANE_SRC_REPEAT_FIRST_FIELD) != 0);
        }

        if(qbinfo.isDisplayInterlaced && qbinfo.isSourceInterlaced)
        {
            /*
             * Only take notice of the interpolation flag if we are on an
             * interlaced display.
             */
            qbinfo.interpolateFields = ((pFrame->src.ulFlags & STM_PLANE_SRC_INTERPOLATE_FIELDS) != 0);
        }
    }
    DEBUGF2(3,("CGammaCompositorPlane::GetQueueBufferInfo: display = %s  source = %s\n",qbinfo.isDisplayInterlaced?"interlaced":"progressive", qbinfo.isSourceInterlaced?"interlaced":"progressive"));

    qbinfo.src = pFrame->src.Rect;
    qbinfo.dst = pFrame->dst.Rect;

    qbinfo.presentation_info = pFrame->info;
    if(qbinfo.presentation_info.nfields == 0)
    {
        // Default to something sensible for 1x playback
        qbinfo.presentation_info.nfields = (qbinfo.isSourceInterlaced && !qbinfo.firstFieldOnly)? 2:1;
        DEBUGF2(3,("%s: default number of fields changed to %d\n", __PRETTY_FUNCTION__, pFrame->info.nfields));
    }


    DEBUGF2(3,("CGammaCompositorPlane::GetQueueBufferInfo: src.x = %ld y = %ld width = %ld height = %ld\n",qbinfo.src.x,qbinfo.src.y,qbinfo.src.width,qbinfo.src.height));
    DEBUGF2(3,("CGammaCompositorPlane::GetQueueBufferInfo: dst.x = %ld y = %ld width = %ld height = %ld\n",qbinfo.dst.x,qbinfo.dst.y,qbinfo.dst.width,qbinfo.dst.height));

    /*
     * Convert the source origin to fixed point format ready for setting up
     * the resize filters. Note that the incoming coordinates can be in either
     * whole integer or multiples of a 16th or a 32nd of a pixel/scanline.
     */
    int multiple;
    switch(pFrame->src.ulFlags & (STM_PLANE_SRC_XY_IN_32NDS|STM_PLANE_SRC_XY_IN_16THS))
    {
      case STM_PLANE_SRC_XY_IN_32NDS:
          multiple = 32;
          break;
      case STM_PLANE_SRC_XY_IN_16THS:
          multiple =16;
          break;
      case 0:
          multiple = 1;
          break;
      default:
          DERROR("invalid buffer flags, both XY_IN_32NDS and XY_IN_16THs set\n");
          return false;
    }

    qbinfo.src.x = ValToFixedPoint(qbinfo.src.x, multiple);
    qbinfo.src.y = ValToFixedPoint(qbinfo.src.y, multiple);


    DEBUGF2(3,("CGammaCompositorPlane::GetQueueBufferInfo: src.x = %ld y = %ld width = %ld height = %ld\n",qbinfo.src.x,qbinfo.src.y,qbinfo.src.width,qbinfo.src.height));
    DEBUGF2(3,("CGammaCompositorPlane::GetQueueBufferInfo: dst.x = %ld y = %ld width = %ld height = %ld\n",qbinfo.dst.x,qbinfo.dst.y,qbinfo.dst.width,qbinfo.dst.height));

    return true;
}


void CGammaCompositorPlane::AdjustBufferInfoForScaling(const stm_display_buffer_t * const pFrame,
                                                       GAMMA_QUEUE_BUFFER_INFO           &qbinfo) const
{
    if(!qbinfo.isSourceInterlaced && qbinfo.isDisplayInterlaced)
    {
        if(m_bHasProgressive2InterlacedHW)
        {
            DEBUGF2(3,("%s: using P2I\n", __PRETTY_FUNCTION__));
            qbinfo.isUsingProgressive2InterlacedHW = true;
        }
        else if (m_ulMaxLineStep > 1)
        {
            DEBUGF2(3,("%s: using line skip (max %lu)\n\n",
                       __PRETTY_FUNCTION__, m_ulMaxLineStep));
        }
        else
        {
            /*
             * without a P2I block we have to convert to interlaced using a
             * 2x downscale. But, if the hardware cannot do that or the overall
             * scale then goes outside the hardware capabilities, we treat the
             * source as interlaced instead.
             */
            int maxverticaldownscale = m_ulMaxVSrcInc/2;

            bool convert_to_interlaced = (qbinfo.dst.height
                                          < (qbinfo.src.height
                                             * m_fixedpointONE
                                             / maxverticaldownscale));
            if(convert_to_interlaced)
            {
                DEBUGF2(3,("%s: converting source to interlaced for "
                           "downscaling\n", __PRETTY_FUNCTION__));
                qbinfo.isSourceInterlaced = true;
                qbinfo.firstFieldType     = GNODE_TOP_FIELD;
            }
        }
    }

    if(qbinfo.isDisplayInterlaced)
    {
        /*
         * Convert the height from frame to field lines. Note that we
         * do not support an "odd" number of lines on an interlaced display,
         * there must always be a matched top and bottom field line. The
         * start line must also be located on a top field to prevent fields
         * getting accidentally swapped.
         */
        qbinfo.dst.height /= 2;
        qbinfo.dst.y      &= 0xfffffffe;
    }

    /*
     * Save the original source height in frame coordinates.
     */
    qbinfo.srcFrameHeight = qbinfo.src.height;

    if(qbinfo.isSourceInterlaced)
    {
        /*
         * For interlaced content we change the source height to the height
         * of each field.
         */
        qbinfo.src.height /= 2;

        /*
         * Also change the vertical start position from frame to field
         * coordinates, unless we are using the hardware de-interlacer in
         * which case we keep it in frame coordinates.
         *
         * Remember that this value is in the fixed point format.
         */
        if(!qbinfo.isHWDeinterlacing)
            qbinfo.src.y /= 2;
    }

    CalculateHorizontalScaling(qbinfo);
    CalculateVerticalScaling(qbinfo);

    /*
     * Save the adjusted destination height in frame coordinates
     */
    qbinfo.dstFrameHeight = (qbinfo.isDisplayInterlaced
                             ? qbinfo.dst.height * 2
                             : qbinfo.dst.height);

    /*
     * Now adjust the source coordinate system to take into account line skipping
     */
    qbinfo.src.y /= qbinfo.line_step;

    /*
     * Define the Y coordinate limit in the source image, used to ensure we
     * do not go outside of the required source image crop when the Y position
     * is adjusted for re-scale purposes.
     */
    qbinfo.maxYCoordinate = ((qbinfo.src.y / m_fixedpointONE)
                             + qbinfo.verticalFilterInputSamples - 1);

    /*
     * Now we have the global scale and adjusted destination, deal with
     * any non-linear scaling to convert between pixel aspect ratios.
     *
     * If this succeeds, then the hsrcinc and chroma_hsrcinc fields will have
     * been changed to reflect the zoom in the "centre" region of the image
     * only.
     */
    if(pFrame->dst.ulFlags & STM_PLANE_DST_CONVERT_TO_16_9_DISPLAY)
        CalculateNonLinearZoom(pFrame,qbinfo);

    CalculateViewport(qbinfo);
}


void CGammaCompositorPlane::CalculateHorizontalScaling(GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    /*
     * Calculate the scaling factors, with one extra bit of precision so we can
     * round the result.
     */
    qbinfo.hsrcinc = (qbinfo.src.width  * m_fixedpointONE * 2) / qbinfo.dst.width;

    if(qbinfo.isSrc420 || qbinfo.isSrc422)
    {
      /*
       * For formats with half chroma, we have to round up or down to an even
       * number, so that the chroma value which is half this value cannot lose
       * precision.
       */
      qbinfo.hsrcinc += 1L<<1;
      qbinfo.hsrcinc &= ~0x3;
      qbinfo.hsrcinc >>= 1;
    }
    else
    {
      /*
       * As chroma is not an issue here just round the result and convert to
       * the correct fixed point format.
       */
      qbinfo.hsrcinc += 1;
      qbinfo.hsrcinc >>= 1;
    }

    bool bRecalculateDstWidth = false;

    if(qbinfo.hsrcinc < m_ulMinHSrcInc)
    {
        qbinfo.hsrcinc = m_ulMinHSrcInc;
        bRecalculateDstWidth = true;
    }

    if(qbinfo.hsrcinc > m_ulMaxHSrcInc)
    {
        qbinfo.hsrcinc = m_ulMaxHSrcInc;
        bRecalculateDstWidth = true;
    }

    /*
     * Chroma src used only for YUV formats on planes that support separate
     * chroma filtering.
     */
    if(qbinfo.isSrc420 || qbinfo.isSrc422)
        qbinfo.chroma_hsrcinc = qbinfo.hsrcinc/2;
    else
        qbinfo.chroma_hsrcinc = qbinfo.hsrcinc;

    if(bRecalculateDstWidth)
      qbinfo.dst.width  = (qbinfo.src.width  * m_fixedpointONE) / qbinfo.hsrcinc;

    DEBUGF2(3,("%s: one = 0x%lx hsrcinc = 0x%lx chsrcinc = 0x%lx\n",__PRETTY_FUNCTION__,m_fixedpointONE, qbinfo.hsrcinc,qbinfo.chroma_hsrcinc));
}


void CGammaCompositorPlane::CalculateVerticalScaling(GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    unsigned long srcFrameHeight;

    qbinfo.line_step = 1;

    /*
     * If we are hardware de-interlacing then the source image is vertically
     * upsampled before it before it gets to the resize filter. If we are
     * using the P2I block then the required number of samples out of the filter
     * is twice the destination height (which is now in field lines).
     */
    qbinfo.verticalFilterOutputSamples = qbinfo.isUsingProgressive2InterlacedHW?(qbinfo.dst.height*2):qbinfo.dst.height;
restart:
    srcFrameHeight = qbinfo.src.height / qbinfo.line_step;
    qbinfo.verticalFilterInputSamples  = qbinfo.isHWDeinterlacing?(srcFrameHeight*2):srcFrameHeight;

    /*
     * Calculate the scaling factors, with one extra bit of precision so we can
     * round the result.
     */
    qbinfo.vsrcinc = (qbinfo.verticalFilterInputSamples * m_fixedpointONE * 2) / qbinfo.verticalFilterOutputSamples;
    if(qbinfo.isSrc420 && !qbinfo.isHWDeinterlacing)
    {
      /*
       * For formats with half vertical chroma, we have to round up or down to
       * an even number, so that the chroma value which is half this value
       * cannot lose precision. When de-interlacing the hardware upsamples the
       * chroma before the resize so it isn't an issue.
       */
      qbinfo.vsrcinc += 1L<<1;
      qbinfo.vsrcinc &= ~0x3;
      qbinfo.vsrcinc >>= 1;
    }
    else
    {
      /*
       * As chroma is not an issue here just round the result and convert to
       * the correct fixed point format.
       */
      qbinfo.vsrcinc += 1;
      qbinfo.vsrcinc >>= 1;
    }

    bool bRecalculateDstHeight = false;

    if(qbinfo.vsrcinc < m_ulMinVSrcInc)
    {
        qbinfo.vsrcinc = m_ulMinVSrcInc;
        bRecalculateDstHeight = true;
    }

    if(qbinfo.vsrcinc > m_ulMaxVSrcInc)
    {
        if(qbinfo.line_step < m_ulMaxLineStep)
        {
            ++qbinfo.line_step;
            goto restart;
        }
        else
        {
            qbinfo.vsrcinc = m_ulMaxVSrcInc;
            bRecalculateDstHeight = true;
        }
    }

    if(qbinfo.isSrc420 && !qbinfo.isHWDeinterlacing)
        qbinfo.chroma_vsrcinc = qbinfo.vsrcinc / 2;
    else
        qbinfo.chroma_vsrcinc = qbinfo.vsrcinc;

    DEBUGF2(3,("%s: one = 0x%lx vsrcinc = 0x%lx cvsrcinc = 0x%lx\n",__PRETTY_FUNCTION__,m_fixedpointONE, qbinfo.vsrcinc,qbinfo.chroma_vsrcinc));

    if(bRecalculateDstHeight)
    {
        qbinfo.dst.height = (qbinfo.verticalFilterInputSamples * m_fixedpointONE) / qbinfo.vsrcinc;
        if(qbinfo.isUsingProgressive2InterlacedHW)
            qbinfo.dst.height /= 2;
    }
}


void CGammaCompositorPlane::CalculateViewport(GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    /*
     * Now we know the destination viewport extents for the
     * compositor/mixer, which may get clipped by the active video area of
     * the display mode.
     */
    qbinfo.viewportStartPixel = STCalculateViewportPixel(qbinfo.pCurrentMode, qbinfo.dst.x);
    qbinfo.viewportStopPixel  = STCalculateViewportPixel(qbinfo.pCurrentMode, qbinfo.dst.x + qbinfo.dst.width - 1);
    /*
     * We need to limit the number of output samples generated to the
     * (possibly clipped) viewport width.
     */
    qbinfo.horizontalFilterOutputSamples = (qbinfo.viewportStopPixel - qbinfo.viewportStartPixel + 1);

    qbinfo.viewportStartLine  = STCalculateViewportLine(qbinfo.pCurrentMode,  qbinfo.dst.y);

    DEBUGF2(3,("%s: samples = %lu startpixel = %lu stoppixel = %lu\n",__PRETTY_FUNCTION__,qbinfo.horizontalFilterOutputSamples, qbinfo.viewportStartPixel, qbinfo.viewportStopPixel));

    /*
     * The viewport line numbering is always frame based, even on
     * an interlaced display
     */
    qbinfo.viewportStopLine = STCalculateViewportLine(qbinfo.pCurrentMode, qbinfo.dst.y + qbinfo.dstFrameHeight - 1);

    DEBUGF2(3,("%s: startline = %lu stopline = %lu\n",__PRETTY_FUNCTION__,qbinfo.viewportStartLine,qbinfo.viewportStopLine));
}


void CGammaCompositorPlane::CalculateNonLinearZoom(const stm_display_buffer_t * const pFrame,
                                                   GAMMA_QUEUE_BUFFER_INFO           &qbinfo) const
{
    /*
     * We must declare our variables before any of the error jumps.
     */
    ULONG DhOnX;
    ULONG source_centre_width;
    ULONG centre_inc;
    ULONG dst_centre_width;
    ULONG non_linear_width;

    /*
     * If the source pixel aspect ratio hasn't been specified, we can't do this.
     */
    if(pFrame->src.PixelAspectRatio.denominator == 0 || pFrame->src.PixelAspectRatio.numerator == 0)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("%s: invalid source pixel aspect ratio %ld:%ld\n",__PRETTY_FUNCTION__,pFrame->src.PixelAspectRatio.numerator,pFrame->src.PixelAspectRatio.denominator));
        goto error_exit;
    }

    /*
     * If the display mode pixel aspect ratio isn't specified for a 16:9 display
     * we don't do this.
     */
    if(qbinfo.pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_16_9].denominator == 0)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("%s: nlz only supported on 16:9 displays\n",__PRETTY_FUNCTION__));
        goto error_exit;
    }

    source_centre_width = (qbinfo.src.width * pFrame->src.ulLinearCenterPercentage)/100;

    /*
     * The destination centre width is the source centre width scaled in
     * proportion to the vertical scale of the whole image and adjusted for the
     * pixel aspect ratio change between source and destination (assuming
     * it is 16:9) so that the centre looks on screen as if the aspect ratio has
     * not changed.
     *
     *                 qbinfo.dstFrameHeight
     * verticalscale = ---------------------
     *                 qbinfo.srcFrameHeight
     *
     * dst_width = src_width * verticalscale *        1         * src_pixel_aspect
     *                                         ----------------
     *                                         dst_pixel_aspect
     *
     */
    dst_centre_width = (source_centre_width*qbinfo.dstFrameHeight)/qbinfo.srcFrameHeight;
    dst_centre_width *= (qbinfo.pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_16_9].denominator *
                         pFrame->src.PixelAspectRatio.numerator);

    dst_centre_width /= (qbinfo.pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_16_9].numerator *
                         pFrame->src.PixelAspectRatio.denominator);

    non_linear_width = qbinfo.dst.width - dst_centre_width;
    if((dst_centre_width == 0) || (non_linear_width < 4))
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("1: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
            qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        goto error_exit;
    }

    /*
     * The total non-linear width must be even, so the region either side of the
     * centre region is the same.
     */
    if((non_linear_width % 2) == 1)
    {
        non_linear_width--;
        dst_centre_width++;
    }

    qbinfo.nlZone1 = non_linear_width / 2;
    qbinfo.nlZone2 = qbinfo.nlZone1 + dst_centre_width;

    centre_inc = (source_centre_width * m_fixedpointONE) / dst_centre_width;

    if(centre_inc > m_ulMaxHSrcInc || centre_inc < m_ulMinHSrcInc)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("2: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
          qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        DEBUGF2(DEBUG_NLZ_LEVEL,("2: ci(%lu) maxhi(%lu) minhi(%lu)\n",centre_inc,m_ulMaxHSrcInc,m_ulMinHSrcInc));
        goto error_exit;
    }

    /*
     * We now have all the parameters required by the equations given in the
     * video pipeline application note on non linear zoom. The problem is that
     * the abbreviated algebra reductions don't work when you write them out and
     * some of the basic assumptions about the steps through the source image
     * are not quite right in our opinion. So we have two sets of _very_ similar
     * equations here.
     */

    /*
     * The hardware doesn't support a negative PDELTA so check for this case,
     * see equations below for why this is the case.
     */
    if(centre_inc < qbinfo.hsrcinc)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("3: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
          qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        DEBUGF2(DEBUG_NLZ_LEVEL,("3: ci(%lu) hsi(%lu)\n",centre_inc,qbinfo.hsrcinc));
        goto error_exit;
    }

    if(m_bUsePiagetAppNoteEquations)
    {
        /*
         * W1          = Zone1 width
         * X           = total destination width
         * HZoom       = the average total zoom factor = m_fixedpointONE/qbinfo.hsrcinc
         * HZoomCentre = the zoom of the centre region = m_fixedpointONE/centre_inc
         * 8192        = m_fixedpointOne
         *
         * PDELTA = 8192 * 1  * HZoom - HZoomCentre *  X
         *                ----  -------------------   ---
         *                W1-1   HZoom x HZoomCentre   W1
         *
         * If we rearrange the term using HZoom and HZoomCentre we get:
         *
         * HZoom - HZoomCentre          1            1
         * ------------------- =  ------------- - -------
         * HZoom * HZoomCentre     HZoomCentre     HZoom
         *
         *
         *       centre_inc       qbinfo.hsrcinc     centre_inc - qbinfo.hsrcinc
         * =   --------------- -  --------------- =  ---------------------------
         *     m_fixedpointONE    m_fixedpointONE         m_fixedpointONE
         *
         * Substitute and rearrange this back in for PDELTA and we get
         *
         * PDELTA =     X * (centre_inc - qbinfo.hsrcinc)
         *          ------------------------------------------
         *                          W1 * (W1-1)
         */

        qbinfo.pdeltaLuma = qbinfo.dst.width * (centre_inc - qbinfo.hsrcinc);
        qbinfo.pdeltaLuma /= qbinfo.nlZone1 * (qbinfo.nlZone1-1);

        /*
         * Danger Will Robinson, Danger! Danger!
         *
         * if pdeltaLuma is odd, then when we divide it by 2 for the chroma step
         * we get an error, and because this value is typically rather small
         * that error is proportionally very large resulting in complete chaos
         * on the display.
         *
         * Finding a general solution for the source centre percentage that will
         * always result in an even pdeltaLuma for any input size, output size,
         * scale, source pixel aspect ratio etc is not possible. Given an
         * adjusted pdelta we cannot work backards to adjust centre_inc and
         * change dst_centre_width, because to do that we need the start
         * increment which depends on the Dh equation below which in turn
         * depends on dst_centre_width - we have too many unknowns.
         *
         * So all we can do now is to round down pdeltaLuma to an even number.
         * This will result in an image that crops the right hand side of the
         * source image. The hardware is tolerant of us not using all of the
         * source pixels we said we needed.
         */
        if(qbinfo.pdeltaLuma % 2 == 1)
          qbinfo.pdeltaLuma--;

        /*
         * The second equation uses a term Dh, defined as:
         *
         * W2 = number of output pixels in the centre reqion, dst_centre_width
         *
         * Dh = (W1-1) * PDELTA * (W1+W2-1)
         *
         * Although we actually require the value Dh
         *                                        --
         *                                        X
         */
        DhOnX = (qbinfo.nlZone1-1) * qbinfo.pdeltaLuma * (qbinfo.nlZone1+dst_centre_width-1);
        DhOnX /= qbinfo.dst.width;
    }
    else
    {
        /*
         * We think, that because the source increment associated with the final
         * output pixel is not used, we should have:
         *
         * PDELTA = (X-1) * (centre_inc - qbinfo.hsrcinc)
         *          ----------------------------------
         *                    W1 * (W1-1)
         */
        qbinfo.pdeltaLuma = (qbinfo.dst.width-1) * (centre_inc - qbinfo.hsrcinc);
        qbinfo.pdeltaLuma /= qbinfo.nlZone1 * (qbinfo.nlZone1-1);

        /*
         * See comments above on why we adjust this to be even.
         */
        if(qbinfo.pdeltaLuma % 2 == 1)
          qbinfo.pdeltaLuma--;

        /*
         * If you use the above definition of Dh, we believe you cannot derive
         * the original version of PDELTA; the (W1+W2-1) term in Dh results in
         * the bottom term of PDELTA becoming (W1+1)*(W1-1). So the app note
         * maths appears to be self inconsistent. We believe the correct term in
         * Dh is (W1+W2) and as this is in part trying to express the total
         * increment across the centre output region and there should be W2
         * output pixels in that region, not W2-1. So the bottom term of the
         * PDELTA stays the same and Dh becomes:
         *
         * Dh = (W1-1) * PDELTA * (W1+W2)
         */
        DhOnX = (qbinfo.nlZone1-1) * qbinfo.pdeltaLuma * (qbinfo.nlZone1+dst_centre_width);
        DhOnX /= qbinfo.dst.width;
    }

    if(DhOnX > qbinfo.hsrcinc)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("4: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
          qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        DEBUGF2(DEBUG_NLZ_LEVEL,("4: ci(%lu) hsi(%lu) pdelta(%lu) dhonx(%lu)\n",centre_inc,qbinfo.hsrcinc,qbinfo.pdeltaLuma,DhOnX));
        goto error_exit;
    }

    qbinfo.nlLumaStartInc = qbinfo.hsrcinc - DhOnX;
    if(qbinfo.nlLumaStartInc > m_ulMaxHSrcInc || qbinfo.nlLumaStartInc < m_ulMinHSrcInc)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("5: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
          qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        DEBUGF2(DEBUG_NLZ_LEVEL,("5: ci(%lu) hsi(%lu) pdelta(%lu) dhonx(%lu)\n",centre_inc,qbinfo.hsrcinc,qbinfo.pdeltaLuma,DhOnX));
        DEBUGF2(DEBUG_NLZ_LEVEL,("5: lsi(%lu) maxsi(%lu) minsi(%lu)\n",qbinfo.nlLumaStartInc,m_ulMaxHSrcInc,m_ulMinHSrcInc));
        goto error_exit;
    }

    if((DhOnX/2) > qbinfo.chroma_hsrcinc)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("6: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
          qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        DEBUGF2(DEBUG_NLZ_LEVEL,("6: ci(%lu) hsi(%lu) pdelta(%lu) dhonx/2(%lu) chsi(%lu)\n",centre_inc,qbinfo.hsrcinc,qbinfo.pdeltaLuma,DhOnX/2,qbinfo.chroma_hsrcinc));
        goto error_exit;
    }

    qbinfo.nlChromaStartInc = qbinfo.chroma_hsrcinc - (DhOnX/2);
    if(qbinfo.nlChromaStartInc > m_ulMaxHSrcInc || qbinfo.nlChromaStartInc < m_ulMinHSrcInc)
    {
        DEBUGF2(DEBUG_NLZ_LEVEL,("7: sw(%lu) lcp(%lu) dw(%lu) sfh(%lu) dfh(%lu) scw(%lu) dcw(%lu) nlw(%lu)\n",qbinfo.src.width,pFrame->src.ulLinearCenterPercentage,
          qbinfo.dst.width, qbinfo.srcFrameHeight,qbinfo.dstFrameHeight,source_centre_width,dst_centre_width,non_linear_width));
        DEBUGF2(DEBUG_NLZ_LEVEL,("7: ci(%lu) hsi(%lu) pdelta(%lu) dhonx(%lu)\n",centre_inc,qbinfo.hsrcinc,qbinfo.pdeltaLuma,DhOnX));
        DEBUGF2(DEBUG_NLZ_LEVEL,("7: csi(%lu) maxsi(%lu) minsi(%lu)\n",qbinfo.nlChromaStartInc,m_ulMaxHSrcInc,m_ulMinHSrcInc));
        goto error_exit;
    }

    /*
     * Now we change the hsrcinc and chroma_hsrcinc to the expected increments
     * for the centre region, it is this that will determine which set of
     * filter coefficients should be used.
     */
    qbinfo.hsrcinc        = centre_inc;
    qbinfo.chroma_hsrcinc = centre_inc/2;

    return;

error_exit:
    /*
     * Make sure non-linear zoom is disabled in case any of the
     * parameters go out of range.
     */
    qbinfo.pdeltaLuma = 0;
    qbinfo.nlZone1 = 0;
    qbinfo.nlZone2 = 0;
}


void CGammaCompositorPlane::CalculateVerticalFilterSetup (
    const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
    int                            firstSampleOffset,
    bool                           doLumaFilter,
    int                           &y,
    int                           &phase,
    int                           &repeat,
    int                           &height) const
{
    DEBUGF2 (3, ("%s: src.y = %ld firstSampleOffset = 0x%08x\n",
               __PRETTY_FUNCTION__, qbinfo.src.y,firstSampleOffset));

    int maxy = qbinfo.maxYCoordinate;
    height   = qbinfo.verticalFilterInputSamples;
    repeat   = 0;

    /*
     * We have been given the first sample's offset from src.y in the original
     * field or frame coordinate system. We now have to convert that to the
     * actual coordinate system taking into account line skipping. We could have
     * done this in the caller, but doing it here makes the code marginally more
     * readable.
     *
     * Note that qbinfo.src.y is already in the correct coordinate system,
     * having been adjusted when any required line skipping was calculated.
     */
    int subpixelpos = qbinfo.src.y + (firstSampleOffset / qbinfo.line_step);

    DEBUGF2 (3, ("        subpixelpos: 0x%08x height: %d dstheight: %ld\n",
                 subpixelpos, height, qbinfo.dst.height));

    if (!doLumaFilter && qbinfo.isSrc420 && !qbinfo.isHWDeinterlacing)
    {
        /*
         * Special case for 420 Chroma filter setup when not using a
         * de-interlacer, which will upsample the chroma before the vertical
         * filter.
         *
         * Adjust the start position by -1/2 because 420 chroma samples are
         * taken half way between two field lines.
         *
         * As we only have half the number of chroma samples,
         * divide the start position and height by 2.
         */
        subpixelpos = (subpixelpos - (m_fixedpointONE/2) ) / 2;
        height      = height / 2;
        maxy        = maxy / 2;
    }

    Get5TapFilterSetup (subpixelpos, height, repeat,
                        doLumaFilter ? qbinfo.vsrcinc : qbinfo.chroma_vsrcinc,
                        m_fixedpointONE);

    y = FixedPointToInteger(subpixelpos, &phase);

    /*
     * If we are deinterlacing and need an odd address as the first sample in
     * the resize filter, we need to adjust the filter repeat; remember we are
     * effectively only reading the even line addresses from memory and the HW
     * is filling in the odd lines.
     */
    if (qbinfo.isHWDeinterlacing && ((y % 2) == 1))
    {
        y--;
        repeat--;
    }

    height = LimitSizeToRegion(y, maxy, height);
}


bool CGammaCompositorPlane::QueueProgressiveContent(void                    *hwSetup,
                                                    ULONG                    hwSetupSize,
                                              const GAMMA_QUEUE_BUFFER_INFO &qbinfo)
{
    stm_plane_node vidNode = {{0}};
    DEBUGF2(3,("CGammaCompositorPlane::queueProgressiveContent\n"));

    vidNode.info     = qbinfo.presentation_info;
    vidNode.nodeType = GNODE_PROGRESSIVE;

    if(!AllocateNodeMemory(vidNode, hwSetupSize))
    {
        DERROR("Cannot allocate node memory");
        return false;
    }

    g_pIOS->MemcpyToDMAArea(&vidNode.dma_area, 0, hwSetup, hwSetupSize);

    if (!AddToDisplayList(vidNode))
    {
        DERROR("Cannot add node to display list");
        ReleaseNode (vidNode);
        return false;
    }

    return true;
}


bool CGammaCompositorPlane::QueueSimpleInterlacedContent(void                    *hwSetup,
                                                         ULONG                    hwSetupSize,
                                                   const GAMMA_QUEUE_BUFFER_INFO &qbinfo)

{
    stm_plane_node vidNode = {{0}};

    /*
     * This method queues either a pair of fields in an interlaced buffer or
     * a single field in that buffer using a single display node. This covers
     * a number of different use cases:
     *
     * 1. An interlaced field pair on an interlaced display, being displayed at
     *    normal speed without any intra-field interpolation. The fields will
     *    be displayed alternately for as long as the display node is valid.
     *
     * 2. A single interlaced field on an interlaced display. The specified
     *    field will get displayed on the first valid display field, usually for
     *    just one field (it is expected that another node with the next field
     *    will be queued behind it during normal speed playback). If the display
     *    node is maintained on the display for more than one field then the
     *    behaviour depends on the interpolateFields flag. If true (i.e. for
     *    slowmotion) then an interpolated other field is displayed, otherwise
     *    the data from the other field in the buffer is displayed (this is the
     *    best choice of a bad lot, because this should only happen if the
     *    queue has starved or some correction for AV sync is being applied).
     *    This use case is intented to support per-field pan and scan vectors,
     *    where the hardware setup needs to be different for each field,
     *    including the repeated first field when doing 3/2 pulldown (eeek).
     *
     * 3. A single interlaced field on a progressive display. The specified
     *    field will be displayed for as long as the display node is valid.
     *
     */
    vidNode.info         = qbinfo.presentation_info;
    vidNode.nodeType     = qbinfo.firstFieldType;
    vidNode.useAltFields = qbinfo.interpolateFields;

    if(qbinfo.repeatFirstField && !qbinfo.firstFieldOnly)
        vidNode.info.nfields++;

    if(!AllocateNodeMemory(vidNode, hwSetupSize))
    {
        DERROR("Cannot allocate node memory");
        return false;
    }

    g_pIOS->MemcpyToDMAArea(&vidNode.dma_area, 0, hwSetup, hwSetupSize);

    if (!AddToDisplayList(vidNode))
    {
        DERROR("Cannot add node to display list");
        ReleaseNode (vidNode);
        return false;
    }

    return true;
}


bool CGammaCompositorPlane::QueueInterpolatedInterlacedContent(void                    *hwSetup,
                                                               ULONG                    hwSetupSize,
                                                         const GAMMA_QUEUE_BUFFER_INFO &qbinfo)

{
    stm_plane_node vidNode = {{0}};

    /*
     * This method queues both interlaced fields in a buffer using individual
     * display nodes. It is used to ensure both fields get displayed for the
     * right amount of time, in the right order in the following situations:
     *
     * 1. We are de-interlacing the content onto a progressive display, so we
     *    need to use the "nodeType" field in different queue enteries to
     *    explicitly indicate which field to program the hardware for. This
     *    is because in a progressive mode we have no display concept of top
     *    and bottom field.
     *
     * 2. We are doing slowmotion using interpolated fields, in this case
     *    we display the first field for half the time (with interpolation
     *    on the "wrong" field) and then the second field for the rest of the
     *    time. Note that that we always honour repeat first field to make
     *    sure we keep in step with the stream's field ordering.
     *
     * Note that we can cope with nfields being an odd number here, in which
     * case the first field is displayed for longer than the second field
     * (note this only makes sense on a progressive display). If nfields is
     * 1 then the second field is "dropped". Obviously if you do this on
     * an interlaced display and in conjuction with the repeat first field flag
     * you are going to get some nasty effects on the display.
     */
    int nsecondfields = qbinfo.presentation_info.nfields / 2;
    int nfirstfields = qbinfo.presentation_info.nfields - nsecondfields;

    vidNode.info = qbinfo.presentation_info;
    vidNode.info.nfields = nfirstfields;
    if(qbinfo.repeatFirstField || nsecondfields != 0)
        vidNode.info.pCompletedCallback = 0;

    vidNode.useAltFields = qbinfo.interpolateFields;
    vidNode.nodeType     = qbinfo.firstFieldType;

    if(!AllocateNodeMemory(vidNode, hwSetupSize))
    {
        DERROR("Cannot allocate node memory");
        return false;
    }

    g_pIOS->MemcpyToDMAArea(&vidNode.dma_area, 0, hwSetup, hwSetupSize);

    if (!AddToDisplayList(vidNode))
    {
        DERROR("Cannot add node1 to display list");
        ReleaseNode (vidNode);
        return false;
    }

    if(nsecondfields != 0)
    {
        /*
         * Queue the second node.
         */
        vidNode.info.pDisplayCallback = 0;
        vidNode.info.nfields = nsecondfields;

        if(!qbinfo.repeatFirstField)
            vidNode.info.pCompletedCallback = qbinfo.presentation_info.pCompletedCallback;

        vidNode.nodeType = (vidNode.nodeType==GNODE_TOP_FIELD)?GNODE_BOTTOM_FIELD:GNODE_TOP_FIELD;

        if(!AllocateNodeMemory(vidNode, hwSetupSize))
        {
            /* FIXME: should probably remove previously queued node(s) */
            DERROR("Cannot allocate node memory");
            return false;
        }

        g_pIOS->MemcpyToDMAArea(&vidNode.dma_area, 0, hwSetup, hwSetupSize);

        if (!AddToDisplayList(vidNode))
        {
            /* FIXME: should probably remove previously queued node(s) */
            DERROR("Cannot add node2 to display list");
            ReleaseNode (vidNode);
            return false;
        }
    }

    if(qbinfo.repeatFirstField)
    {
        /*
         * Handle the repeat first field flag for 3/2 pulldown. Note that we
         * do this even in "slow motion" to maintain the field ordering.
         */
        DEBUGF2(3,("CGammaCompositorPlane::queueInterpolatedInterlacedContent 'Repeat First Field'\n"));
        vidNode.info.nfields = 1;
        vidNode.info.pCompletedCallback = qbinfo.presentation_info.pCompletedCallback;
        vidNode.nodeType = qbinfo.firstFieldType;

        if(!AllocateNodeMemory(vidNode, hwSetupSize))
        {
            /* FIXME: should probably remove previously queued node(s) */
            DERROR("Cannot allocate node memory");
            return false;
        }

        g_pIOS->MemcpyToDMAArea(&vidNode.dma_area, 0, hwSetup, hwSetupSize);

        if (!AddToDisplayList(vidNode))
        {
            /* FIXME: should probably remove previously queued node(s) */
            DERROR("Cannot add node3 to display list");
            ReleaseNode (vidNode);
            return false;
        }
    }

    return true;
}


bool CGammaCompositorPlane::SetControl(stm_plane_ctrl_t control, ULONG value)
{
    DEBUGF2(2,(FENTRY ": control = %d ulNewVal = %lu (0x%08lx)\n",__PRETTY_FUNCTION__,(int)control,value,value));
    return false;
}


bool CGammaCompositorPlane::GetControl(stm_plane_ctrl_t control, ULONG *value) const
{
    DEBUGF2(2,(FENTRY ": control = %d\n",__PRETTY_FUNCTION__,(int)control));
    return false;
}


stm_plane_caps_t CGammaCompositorPlane::GetCapabilities(void) const
{
    DENTRY();
    return m_capabilities;
}
