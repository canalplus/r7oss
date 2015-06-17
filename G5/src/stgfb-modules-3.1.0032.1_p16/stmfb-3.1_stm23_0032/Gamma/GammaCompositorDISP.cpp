/***********************************************************************
 *
 * File: ./Gamma/GammaCompositorDISP.cpp
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
#include <Generic/Output.h>

#include <STMCommon/stmfilter.h>

#include "GenericGammaDefs.h"
#include "GenericGammaReg.h"

#include "GammaCompositorDISP.h"

#include "GammaDisplayFilter.h"

/*
 * A set of useful named constants to make the filter setup code more readable
 */
static const int fixedpointONE        = 1<<13;
static const int fixedpointHALF       = fixedpointONE/2;
static const int fixedpointQUARTER    = fixedpointONE/4;
static const int isLumaFilter         = true;
static const int isChromaFilter       = !isLumaFilter;
static const int isBottomFieldAddress = true;
static const int isTopFieldAddress    = !isBottomFieldAddress;
static const int isFrameAddress       = !isBottomFieldAddress;


static const SURF_FMT g_surfaceFormats[] = {
    SURF_YCBCR422R,
    SURF_YCBCR422MB,
    SURF_YCBCR420MB
};


CGammaCompositorDISP::CGammaCompositorDISP(stm_plane_id_t   GDPid,
                                           CGammaVideoPlug *videoPlug,
                                           ULONG            dispBaseAddr): CGammaCompositorVideoPlane(GDPid,videoPlug,dispBaseAddr)
{
    DEBUGF2(2,("CGammaCompositorDISP::CGammaCompositorDISP - in\n"));

    m_pSurfaceFormats = g_surfaceFormats;
    m_nFormats = sizeof(g_surfaceFormats)/sizeof(SURF_FMT);

    m_capabilities.ulCaps      = (0
                                  | PLANE_CAPS_RESIZE
                                  | PLANE_CAPS_SUBPIXEL_POSITION
                                  | PLANE_CAPS_SRC_COLOR_KEY
                                  | PLANE_CAPS_GLOBAL_ALPHA
                                  | PLANE_CAPS_DEINTERLACE
                                 );
    m_capabilities.ulControls  = PLANE_CTRL_CAPS_NONE;
    m_capabilities.ulMinFields = 1;
    m_capabilities.ulMinWidth  = 32;
    m_capabilities.ulMinHeight = 24;
    m_capabilities.ulMaxWidth  = 960; /* This is for the SD version of the block */
    m_capabilities.ulMaxHeight = 576;
    SetScalingCapabilities(&m_capabilities);

    // Control register, everything disabled
    WriteVideoReg(DISP_CTL,0x00000000);

    /*
     * Maximum packet size - stbus configuration
     * We are little endian, and require message size stbus transactions
     */
    WriteVideoReg(DISP_PKZ, 0);

    DEBUGF2(2,("CGammaCompositorDISP::CGammaCompositorDISP - out\n"));
}


bool CGammaCompositorDISP::QueueBuffer(const stm_display_buffer_t * const pFrame,
                                       const void                 * const user)
{
    GammaDISPSetup          displayNode = {0};
    GAMMA_QUEUE_BUFFER_INFO qbinfo      = {0};

    DEBUGF2 (5,("%s\n", __PRETTY_FUNCTION__));

    if(!CGammaCompositorPlane::GetQueueBufferInfo(pFrame, user, qbinfo))
      return false;

    /*
     * Change the src and destination rectangles ready for
     * scaling and interlaced<->progressive conversions. Also
     * calculates the sample rate converter increments for
     * the scaling hardware.
     */
    CGammaCompositorPlane::AdjustBufferInfoForScaling(pFrame, qbinfo);

    // Basic control setup
    displayNode.CTL = DISP_CTL__OUTPUT_ENABLE;

    if(!m_videoPlug->CreatePlugSetup(displayNode.vidPlugSetup, pFrame, qbinfo))
        return false;

    setMemoryAddressing(displayNode, pFrame, qbinfo);

    selectScalingFilters(displayNode, qbinfo);

    if(qbinfo.isSrcMacroBlock)
        setupMBHorizontalScaling(displayNode, qbinfo);
    else
        setup422RHorizontalScaling(displayNode, qbinfo);

    if(!qbinfo.isSourceInterlaced)
    {
        /*
         * Progressive content on either a progressive or interlaced display
         */
        setupProgressiveVerticalScaling(displayNode, qbinfo);

        return QueueProgressiveContent(&displayNode, sizeof(displayNode), qbinfo);
    }

    if(!qbinfo.isDisplayInterlaced)
        setupDeInterlacedVerticalScaling(displayNode, qbinfo);
    else
        setupInterlacedVerticalScaling(displayNode, qbinfo);

    if(qbinfo.firstFieldOnly || (qbinfo.isDisplayInterlaced && !qbinfo.interpolateFields))
        return QueueSimpleInterlacedContent(&displayNode, sizeof(displayNode), qbinfo);
    else
        return QueueInterpolatedInterlacedContent(&displayNode, sizeof(displayNode), qbinfo);
}


void CGammaCompositorDISP::DisableHW()
{
    if(m_isEnabled)
    {
        // Disable video display processor
        WriteVideoReg(DISP_CTL, 0);

        CDisplayPlane::DisableHW();
    }
}


void CGammaCompositorDISP::UpdateHW(bool isDisplayInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const TIME64 &vsyncTime)
{
    /*
     * This is only called as part of the display interrupt processing
     * bottom half and is not re-enterant. Given limitations on stack usage in
     * interrupt context on certain systems, we keep as much off the stack
     * as possible.
     */
    static stm_plane_node  displayNode;
    static DISPFieldSetup *nextfieldsetup;

    /*
     * PART1
     *
     * Update the state with what is currently on the display, i.e. is
     * the current node still there because it was to be displayed for
     * several fields, or has it finished and is a new node being displayed
     */
    UpdateCurrentNode(vsyncTime);


    /*
     * PART2.
     *
     * After the update we need to check the current node status again.
     *
     * Firstly, if we are on an interlaced display and we still have a valid
     * node, AND we are NOT paused, we need to update the field specific
     * hardware setup for the next field. This copes with the trick mode case
     * and the case where there is nothing left to display but the current node
     * is persistent. It doesn't matter if we do this and then PART3 programs
     * a completely new node instead.
     */
    nextfieldsetup = 0;

    if(m_currentNode.isValid && isDisplayInterlaced)
        nextfieldsetup = SelectFieldSetup(isTopFieldOnDisplay, m_currentNode);

    if(m_isPaused || (m_currentNode.isValid && m_currentNode.info.nfields > 1))
    {
        /*
         * We have got a node on display that is going to be there for more than
         * one frame/field, so we do not get another from the queue yet.
         */
        goto write_next_field;
    }

    /*
     * PART3.
     *
     * For whatever reason we now want to try and queue a new node to be
     * displayed on the NEXT field/frame interrupt.
     */
    if(PeekNextNodeFromDisplayList(vsyncTime, displayNode))
    {
        if(isDisplayInterlaced)
        {
            /*
             * If we are being asked to present graphics on an interlaced display
             * then only allow changes on the top field. This is to prevent visual
             * artifacts when animating vertical movement.
             */
            if(isTopFieldOnDisplay && (displayNode.info.ulFlags & STM_PLANE_PRESENTATION_GRAPHICS))
                goto write_next_field;

            /*
             * On an interlaced display, we need to deal with the situation
             * where the node we have dequeued is marked as belonging to one
             * field but we are updating for a different field. There are two
             * situations where this can happen, firstly some error has caused
             * us to miss processing a field interrupt so we are out of
             * sequence. Secondly we are in a startup condition, where the
             * stream is starting on the opposite field to the current hardware
             * state. Progressive nodes can be displayed on any field.
             *
             * In both cases we keep the existing node (if any) on the display
             * for another field. Note that there are two versions depending
             * on if the force field flag has been set.
             */
            if(!displayNode.useAltFields)
            {
                if((isTopFieldOnDisplay  && displayNode.nodeType == GNODE_TOP_FIELD) ||
                   (!isTopFieldOnDisplay && displayNode.nodeType == GNODE_BOTTOM_FIELD))
                {
                    DEBUGF2(3,("CGammaCompositorDISP::updateHW Waiting for correct field\n"));
                    DEBUGF2(3,("CGammaCompositorDISP::updateHW isTopFieldOnDisplay = %s\n", isTopFieldOnDisplay?"true":"false"));
                    DEBUGF2(3,("CGammaCompositorDISP::updateHW nodeType            = %s\n", (displayNode.nodeType==GNODE_TOP_FIELD)?"top":"bottom"));

                    goto write_next_field;
                }
            }

            nextfieldsetup = SelectFieldSetup(isTopFieldOnDisplay,displayNode);
        }
        else
        {
            if(displayNode.nodeType == GNODE_PROGRESSIVE || displayNode.nodeType == GNODE_TOP_FIELD)
                nextfieldsetup = &((GammaDISPSetup*)displayNode.dma_area.pData)->topField;
            else
                nextfieldsetup = &((GammaDISPSetup*)displayNode.dma_area.pData)->botField;
        }

        /*
         * Write all display pipe registers out before enabling the plane on
         * the mixer. Note that the last thing WriteCommonSetup does is write
         * to the pipeline control register.
         */
        WriteFieldSetup(nextfieldsetup);
        WriteCommonSetup((GammaDISPSetup*)displayNode.dma_area.pData);

        EnableHW();
        /*
         * Check if something went wrong during hardware enable, if so clear
         * the display pipe hardware control register again and keep this
         * buffer on the queue to try again later.
         */
        if(!m_isEnabled)
        {
          WriteVideoReg(DISP_CTL, 0);
          return;
        }

        SetPendingNode(displayNode);
        PopNextNodeFromDisplayList();
        return;
    }

    /*
     * Finally write the next field setup if required. This ensures we only do
     * it once in the case where a persistent frame is on display but we now
     * have a new node.
     */
write_next_field:
    if(nextfieldsetup)
        WriteFieldSetup(nextfieldsetup);

}


////////////////////////////////////////////////////////////////////////////
// Video Display Processor private implementation
DISPFieldSetup *CGammaCompositorDISP::SelectFieldSetup(bool isTopFieldOnDisplay,
                                                       stm_plane_node  &displayNode)
{
    if(!isTopFieldOnDisplay)
    {
        DEBUGF2(4,("CGammaCompositorDISP::SelectFieldSetup Updating for top field\n"));
        if(m_isPaused || displayNode.useAltFields)
        {
            if(displayNode.nodeType == GNODE_TOP_FIELD)
                return &((GammaDISPSetup*)displayNode.dma_area.pData)->topField;
            else
                return &((GammaDISPSetup*)displayNode.dma_area.pData)->altTopField;
        }
        else
        {
            return &((GammaDISPSetup*)displayNode.dma_area.pData)->topField;
        }

    }
    else
    {
        DEBUGF2(4,("CGammaCompositorDISP::SelectFieldSetup Updating for bottom field\n"));
        if(m_isPaused || displayNode.useAltFields)
        {
            if(displayNode.nodeType == GNODE_BOTTOM_FIELD)
                return &((GammaDISPSetup*)displayNode.dma_area.pData)->botField;
            else
                return &((GammaDISPSetup*)displayNode.dma_area.pData)->altBotField;
        }
        else
        {
            return &((GammaDISPSetup*)displayNode.dma_area.pData)->botField;
        }
    }
}


void CGammaCompositorDISP::WriteFieldSetup(DISPFieldSetup *field)
{
    DEBUGF2(4,("CGammaCompositorDISP::WriteFieldSetup\n"));
    WriteVideoReg(DISP_LUMA_BA,   field->LUMA_BA);
    WriteVideoReg(DISP_LUMA_VSRC, field->LUMA_VSRC);
    WriteVideoReg(DISP_CHR_VSRC,  field->CHR_VSRC);
    WriteVideoReg(DISP_LUMA_XY,   field->LUMA_XY);
    WriteVideoReg(DISP_CHR_XY,    field->CHR_XY);
    WriteVideoReg(DISP_LUMA_SIZE, field->LUMA_SIZE);
    WriteVideoReg(DISP_CHR_SIZE,  field->CHR_SIZE);

    DEBUGF2(4,("\tDISP_LUMA_BA:\t%#08lx\n",   field->LUMA_BA));
    DEBUGF2(4,("\tDISP_LUMA_VSRC:\t%#08lx\n", field->LUMA_VSRC));
    DEBUGF2(4,("\tDISP_CHR_VSRC:\t%#08lx\n",  field->CHR_VSRC));
    DEBUGF2(4,("\tDISP_LUMA_XY:\t%#08lx\n",   field->LUMA_XY));
    DEBUGF2(4,("\tDISP_CHR_XY:\t%#08lx\n",    field->CHR_XY));
    DEBUGF2(4,("\tDISP_LUMA_SIZE:\t%#08lx\n", field->LUMA_SIZE));
    DEBUGF2(4,("\tDISP_CHR_SIZE:\t%#08lx\n",  field->CHR_SIZE));
}


void CGammaCompositorDISP::WriteCommonSetup(GammaDISPSetup *setup)
{
    DEBUGF2(4,("CGammaCompositorDISP::WriteCommonSetup\n"));

    WriteVideoReg(DISP_MA_CTL,      setup->MA_CTL);
    WriteVideoReg(DISP_TARGET_SIZE, setup->TARGET_SIZE);
    WriteVideoReg(DISP_CHR_BA,      setup->CHR_BA);
    WriteVideoReg(DISP_PMP,         setup->PMP);
    WriteVideoReg(DISP_LUMA_HSRC,   setup->LUMA_HSRC);
    WriteVideoReg(DISP_CHR_HSRC,    setup->CHR_HSRC);

    WriteVideoReg(DISP_NLZZD_Y,     setup->NLZZD);
    WriteVideoReg(DISP_NLZZD_C,     setup->NLZZD);
    WriteVideoReg(DISP_PDELTA,      setup->PDELTA);


    DEBUGF2(4,("\tDISP_CTL:\t%#08lx\n",         setup->CTL));
    DEBUGF2(4,("\tDISP_MA_CTL:\t%#08lx\n",      setup->MA_CTL));
    DEBUGF2(4,("\tDISP_TARGET_SIZE:\t%#08lx\n", setup->TARGET_SIZE));
    DEBUGF2(4,("\tDISP_CHR_BA:\t%#08lx\n",      setup->CHR_BA));
    DEBUGF2(4,("\tDISP_PMP:\t%#08lx\n",         setup->PMP));
    DEBUGF2(4,("\tDISP_LUMA_HSRC:\t%#08lx\n",   setup->LUMA_HSRC));
    DEBUGF2(4,("\tDISP_CHR_HSRC:\t%#08lx\n",    setup->CHR_HSRC));
    DEBUGF2(4,("\tDISP_NLZZD_*:\t%#08lx\n",     setup->NLZZD));
    DEBUGF2(4,("\tDISP_PDELTA:\t%#08lx\n",      setup->PDELTA));

    if(setup->hfluma)
    {
        // Copy the required filter coefficients to the hardware filter tables
        g_pIOS->MemcpyToDMAArea(&m_HFilter, 0,            setup->hfluma,   HFILTER_SIZE);
        g_pIOS->MemcpyToDMAArea(&m_HFilter, HFILTER_SIZE, setup->hfchroma, HFILTER_SIZE);
        g_pIOS->MemcpyToDMAArea(&m_VFilter, 0,            setup->vfluma,   VFILTER_SIZE);
        g_pIOS->MemcpyToDMAArea(&m_VFilter, VFILTER_SIZE, setup->vfchroma, VFILTER_SIZE);

        // Program the hardware to load the new tables
        WriteVideoReg(DISP_HFP, m_HFilter.ulPhysical);
        WriteVideoReg(DISP_VFP, m_VFilter.ulPhysical);
    }

    m_videoPlug->WritePlugSetup(setup->vidPlugSetup);

    /*
     * Finally write the control register to turn the pipeline on.
     */
    WriteVideoReg(DISP_CTL, setup->CTL);
}


/*
 * Resize filter setup for the video display pipeline used by QueueBuffer.
 *
 * Be afraid ... be very afraid!
 */
void CGammaCompositorDISP::selectScalingFilters(GammaDISPSetup &displayNode,
                                                const GAMMA_QUEUE_BUFFER_INFO &qbinfo)
{
    DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters\n"));

    DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters src.width = %ld dst.width = %ld\n",qbinfo.src.width,qbinfo.dst.width));
    DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters src.height = %ld dst.height = %ld\n",qbinfo.src.height,qbinfo.dst.height));
    DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters hsrcinc = %lu vsrcinc = %lu\n",qbinfo.hsrcinc,qbinfo.vsrcinc));

    if(m_HVSRCState.LumaHInc != qbinfo.hsrcinc ||
       m_HVSRCState.LumaVInc != qbinfo.vsrcinc)
    {
        /*
         * We put the filter change on both the top and bottom nodes because
         * we don't know which will get displayed first.
         */
        displayNode.CTL |= (DISP_CTL__LOAD_HFILTER_COEFFS |
                            DISP_CTL__LOAD_VFILTER_COEFFS);


        displayNode.hfluma   = GammaDisplayFilter::SelectHorizontalFilter(qbinfo.hsrcinc);
        displayNode.hfchroma = GammaDisplayFilter::SelectHorizontalFilter(qbinfo.chroma_hsrcinc);
        displayNode.vfluma   = GammaDisplayFilter::SelectVerticalFilter(qbinfo.vsrcinc);
        displayNode.vfchroma = GammaDisplayFilter::SelectVerticalFilter(qbinfo.chroma_vsrcinc);

        DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters 'hfluma = %p hfchroma = %p'\n",displayNode.hfluma,displayNode.hfchroma));
        DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters 'vfluma = %p vfchroma = %p'\n",displayNode.vfluma,displayNode.vfchroma));

        m_HVSRCState.LumaHInc = qbinfo.hsrcinc;
        m_HVSRCState.LumaVInc = qbinfo.vsrcinc;
    }

    /*
     * For the horizontal filter, with no scaling (i.e. a src increment of 1),
     * we must use the internal coefficients. Hence we only turn on the
     * horizontal filter when the increment is not one.
     */
    if (qbinfo.hsrcinc != (ULONG)m_fixedpointONE)
        displayNode.CTL |= DISP_CTL__LUMA_HFILTER_ENABLE;

    if (qbinfo.chroma_hsrcinc != (ULONG)m_fixedpointONE)
        displayNode.CTL |= DISP_CTL__CHROMA_HFILTER_ENABLE;

    DEBUGF2(3,("CGammaCompositorDISP::selectScalingFilters - out\n"));
}


void CGammaCompositorDISP::setup422RHorizontalScaling(GammaDISPSetup   &displayNode,
                                                      const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2(3,("CGammaCompositorDISP::setup422RHorizontalScaling\n"));

    /*
     * Horizontal filter setup is reasonably simple and is the same for both
     * fields when dealing with the source as two interlaced fields.
     *
     * First of all get the startposition in fixed point format. This is used
     * as the basis for both the luma and chroma filter setup.
     */
    int subpixelpos;
    int width  = qbinfo.src.width;
    int repeat = 3;
    int x      = FixedPointToInteger(qbinfo.src.x, &subpixelpos);

    displayNode.LUMA_HSRC = PackRegister((subpixelpos | (repeat<<13)),                                   // Hi 16 bits
                                         (qbinfo.pdeltaLuma == 0)?qbinfo.hsrcinc:qbinfo.nlLumaStartInc); // Lo 16 bits

    /*
     * This initializes the position and size register value, and therefore
     * this routine must be called before setting up the vertical scaling,
     * which modifies the top 16bits of these values for y position and height.
     *
     * The horizontal parameters do not change for top and bottom fields.
     */
    displayNode.topField.LUMA_SIZE    = (width & 0xffff);
    displayNode.botField.LUMA_SIZE    = (width & 0xffff);
    displayNode.altTopField.LUMA_SIZE = (width & 0xffff);
    displayNode.altBotField.LUMA_SIZE = (width & 0xffff);

    displayNode.topField.LUMA_XY      = (x & 0xffff);
    displayNode.botField.LUMA_XY      = (x & 0xffff);
    displayNode.altTopField.LUMA_XY   = (x & 0xffff);
    displayNode.altBotField.LUMA_XY   = (x & 0xffff);

    /*
     * Now for the chroma, which always has half the number of samples
     * horizontally than the luma. Note that the first chroma sample
     * is at the same position as the first luma sample when x is even; but
     * when x is odd, we need to delay it by another sample (by repeating the
     * first chroma pixel one extra time), as the first chroma sample is the
     * one to the right of the luma sample.
     */
    if((x % 2) != 0)
        repeat = 4;

    x = FixedPointToInteger((qbinfo.src.x/2), &subpixelpos);

    displayNode.CHR_HSRC = PackRegister((subpixelpos | (repeat<<13)),                                            // Hi 16 bits
                                        (qbinfo.pdeltaLuma == 0)?qbinfo.chroma_hsrcinc:qbinfo.nlChromaStartInc); // Lo 16 bits

    /*
     * The Chroma position and size registers are unused in 422R
     */

    displayNode.PDELTA = PackRegister(qbinfo.pdeltaLuma, qbinfo.pdeltaLuma/2);
    displayNode.NLZZD  = PackRegister(qbinfo.nlZone1, qbinfo.nlZone2);

    DEBUGF2(3,("CGammaCompositorDISP::setup422RHorizontalScaling - out\n"));
}


void CGammaCompositorDISP::setupMBHorizontalScaling(GammaDISPSetup   &displayNode,
                                                    const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2(3,("CGammaCompositorDISP::setupMBHorizontalScaling\n"));

    /*
     * Horizontal filter setup is reasonably simple and is the same for both
     * fields when dealing with the source as two interlaced fields.
     *
     * First of all get the startposition in fixed point format. This is used
     * as the basis for both the luma and chroma filter setup.
     */
    int subpixelpos;
    int width  = qbinfo.src.width;
    int repeat = 3;
    int x      = FixedPointToInteger(qbinfo.src.x, &subpixelpos);

    DEBUGF2(3,("\tluma filter tap setup x = %d subpixelpos = 0x%08d width = %d repeat = %d\n",x,subpixelpos,width,repeat));

    displayNode.LUMA_HSRC = PackRegister((subpixelpos | (repeat<<13)),                                   // Hi 16 bits
                                         (qbinfo.pdeltaLuma == 0)?qbinfo.hsrcinc:qbinfo.nlLumaStartInc); // Lo 16 bits

    /*
     * This initializes the position and size register value, and therefore
     * this routine must be called before setting up the vertical scaling,
     * which modifies the top 16bits of these values for y position and height.
     *
     * The horizontal parameters do not change for top and bottom fields.
     *
     * Round up the Luma width to be a multiple of 8 to avoid an issue
     * on some platforms.
     */
    if((width & 0x7) != 0)
      width = (width+0x7) & 0xfffffff8;

    displayNode.topField.LUMA_SIZE    = (width & 0xffff);
    displayNode.botField.LUMA_SIZE    = (width & 0xffff);
    displayNode.altTopField.LUMA_SIZE = (width & 0xffff);
    displayNode.altBotField.LUMA_SIZE = (width & 0xffff);

    displayNode.topField.LUMA_XY      = (x & 0xffff);
    displayNode.botField.LUMA_XY      = (x & 0xffff);
    displayNode.altTopField.LUMA_XY   = (x & 0xffff);
    displayNode.altBotField.LUMA_XY   = (x & 0xffff);

    /*
     * Now for the chroma, which always has half the number of samples
     * horizontally than the luma. Note that the first chroma sample
     * is at the same position as the first luma sample.
     */
    width  = qbinfo.src.width/2;
    repeat = 3;
    x      = FixedPointToInteger((qbinfo.src.x/2), &subpixelpos);

    DEBUGF2(3,("\tchroma filter tap setup x = %d subpixelpos = 0x%08d width = %d repeat = %d\n",x,subpixelpos,width,repeat));

    displayNode.CHR_HSRC = PackRegister((subpixelpos | (repeat<<13)),                                            // Hi 16 bits
                                        (qbinfo.pdeltaLuma == 0)?qbinfo.chroma_hsrcinc:qbinfo.nlChromaStartInc); // Lo 16 bits

    /*
     * Round up the Chroma width to be a multiple of 4 to avoid an issue
     * on some platforms.
     */
    if((width & 0x3) != 0)
      width = (width+0x3) & 0xfffffffc;

    displayNode.topField.CHR_SIZE    = (width & 0xffff);
    displayNode.botField.CHR_SIZE    = (width & 0xffff);
    displayNode.altTopField.CHR_SIZE = (width & 0xffff);
    displayNode.altBotField.CHR_SIZE = (width & 0xffff);

    displayNode.topField.CHR_XY      = (x & 0xffff);
    displayNode.botField.CHR_XY      = (x & 0xffff);
    displayNode.altTopField.CHR_XY   = (x & 0xffff);
    displayNode.altBotField.CHR_XY   = (x & 0xffff);

    displayNode.PDELTA = PackRegister(qbinfo.pdeltaLuma, qbinfo.pdeltaLuma/2);
    displayNode.NLZZD  = PackRegister(qbinfo.nlZone1, qbinfo.nlZone2);

    DEBUGF2(3,("CGammaCompositorDISP::setupMBHorizontalScaling - out\n"));
}


void CGammaCompositorDISP::calculateVerticalFilterSetup(DISPFieldSetup         &field,
                                                 const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                                                        int                     firstSampleOffset,
                                                        bool                    doLumaFilter,
                                                        bool                    isBottomField) const

{
    int y,phase,repeat,height;

    CGammaCompositorPlane::CalculateVerticalFilterSetup(qbinfo,
                                                        firstSampleOffset,
                                                        doLumaFilter,
                                                        y,
                                                        phase,
                                                        repeat,
                                                        height);

    /*
     * If we are using hardware field access in macroblock formats, we need
     * to convert the y which is currently a field line number to a frame line
     * number. Firstly we need to multiply y by 2, then if it this is for a
     * bottom field we need to add 1.
     */
    if(qbinfo.isSrcMacroBlock && qbinfo.isSourceInterlaced)
    {
        y *= 2;
        if(isBottomField)
            y++;
    }

    if(doLumaFilter)
    {
        DEBUGF2 (3, ("luma_filter: y: %d phase: 0x%08x height: %d repeat: %d srcinc: 0x%08x\n",
                     y, phase, height, repeat, (int)qbinfo.vsrcinc));

        field.LUMA_XY   |= (y      << 16);
        field.LUMA_SIZE |= (height << 16);
        field.LUMA_VSRC  = (repeat << 29) | (phase << 16) | qbinfo.vsrcinc;
    }
    else
    {
        DEBUGF2 (3, ("chroma_filter: y: %d phase: 0x%08x height: %d repeat: %d srcinc: 0x%08x\n",
                     y, phase, height, repeat, (int)qbinfo.chroma_vsrcinc));

        field.CHR_XY   |= (y      << 16);
        field.CHR_SIZE |= (height << 16);
        field.CHR_VSRC  = (repeat << 29) | (phase << 16) | qbinfo.chroma_vsrcinc;
    }

}


void CGammaCompositorDISP::setupProgressiveVerticalScaling(GammaDISPSetup          &displayNode,
                                                     const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    int bottomFieldAdjustment = ScaleVerticalSamplePosition(fixedpointONE, qbinfo);

    /*
     * For progressive content we only need a single node, even on an interlaced
     * display, as we use the top and bottom field setup in the node.Note that
     * the bottom field values will never be used when displaying on a
     * progressive display.
     *
     * First Luma
     */
    calculateVerticalFilterSetup(displayNode.topField, qbinfo, 0, isLumaFilter, isFrameAddress);
    calculateVerticalFilterSetup(displayNode.botField, qbinfo, bottomFieldAdjustment, isLumaFilter, isFrameAddress);

    /*
     * Now Chroma
     */
    calculateVerticalFilterSetup(displayNode.topField, qbinfo, 0, isChromaFilter, isFrameAddress);
    calculateVerticalFilterSetup(displayNode.botField, qbinfo, bottomFieldAdjustment, isChromaFilter, isFrameAddress);

    /*
     * The alternate field setups (for inter-field interpolation) are identical
     * to the main ones for progressive content. This keeps the presentation
     * logic simpler.
     */
    displayNode.altTopField = displayNode.topField;
    displayNode.altBotField = displayNode.botField;
}


void CGammaCompositorDISP::setupDeInterlacedVerticalScaling(GammaDISPSetup          &displayNode,
                                                      const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * We de-interlace, without having a real hardware de-interlacer,
     * by scaling up each field to produce a whole frame. To make consecutive
     * frames appear to be in the same place we move the source start position
     * by +/-1/4 of the distance between two source lines in the SAME field.
     *
     */
    int topSamplePosition =  fixedpointQUARTER;
    int botSamplePosition = -fixedpointQUARTER;

    /*
     * Luma
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, topSamplePosition, isLumaFilter, isTopFieldAddress);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, isLumaFilter, isBottomFieldAddress);

    /*
     * Chroma
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, topSamplePosition, isChromaFilter, isTopFieldAddress);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, isChromaFilter, isBottomFieldAddress);

    /*
     * As this must be presented on a progressive display the alternate field
     * setups are never needed.
     */
}


void CGammaCompositorDISP::setupInterlacedVerticalScaling(GammaDISPSetup          &displayNode,
                                                    const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * The first sample position to interpolate a top field from the bottom
     * field source data is just a coordinate translation between the top and
     * bottom source fields.
     */
    int altTopSamplePosition = -fixedpointHALF;

    /*
     * The first sample position for the _display's_ bottom field, interpolated
     * from the top field data, is +1/2 (but scaled this time). This is because
     * the distance between the source samples located on two consecutive top
     * field display lines has changed; the bottom field display line sample
     * must lie half way between them in the source coordinate system.
     */
    int altBotSamplePosition = ScaleVerticalSamplePosition(fixedpointHALF, qbinfo);

    /*
     * You might think that the sample position for the bottom field generated
     * from real bottom field data should always be 0. But this is quite
     * complicated, because as we have seen above when the image is scaled the
     * mapping of the bottom field's first display line back to the source
     * image changes. Plus we have a source coordinate system change because
     * the first sample of the bottom field data is referenced as 0 just as the
     * first sample of the top field data is.
     *
     * So the start position is the source sample position, of the display's
     * bottom field in the top field's coordinate system, translated to the
     * bottom field's coordinate system.
     */
    int botSamplePosition = altBotSamplePosition - fixedpointHALF;

    /*
     * Work out the real top and bottom fields
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, 0, isLumaFilter, isTopFieldAddress);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, isLumaFilter, isBottomFieldAddress);

    /*
     * Now interpolate a bottom field from the top field contents in
     * order to do slowmotion and pause without motion jitter.
     */
    calculateVerticalFilterSetup (displayNode.altBotField, qbinfo,altBotSamplePosition, isLumaFilter, isTopFieldAddress);

    /*
     * Now interpolate a top field from the bottom field contents.
     */
    calculateVerticalFilterSetup (displayNode.altTopField, qbinfo, altTopSamplePosition, isLumaFilter, isBottomFieldAddress);

    /*
     * Now do exactly the same again for the chroma.
     */
    calculateVerticalFilterSetup (displayNode.topField,    qbinfo, 0,                    isChromaFilter, isTopFieldAddress);
    calculateVerticalFilterSetup (displayNode.botField,    qbinfo, botSamplePosition,    isChromaFilter, isBottomFieldAddress);
    calculateVerticalFilterSetup (displayNode.altBotField, qbinfo, altBotSamplePosition, isChromaFilter, isTopFieldAddress);
    calculateVerticalFilterSetup (displayNode.altTopField, qbinfo, altTopSamplePosition, isChromaFilter, isBottomFieldAddress);
}


/*
 * General setup helper functions for QueueBuffer
 */
void CGammaCompositorDISP::setMemoryAddressing(GammaDISPSetup                &displayNode,
                                               const stm_display_buffer_t    * const pFrame,
                                               const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    // Initialise memory control with some standard values
    displayNode.MA_CTL = DISP_MA_CTL__COEF_LOAD_LINE_NUM(2) |
                         DISP_MA_CTL__PIX_LOAD_LINE_NUM(4)  |
                         DISP_MA_CTL__MIN_MEM_REQ_INTERVAL(0);

    ULONG baseAddress   = pFrame->src.ulVideoBufferAddr;
    ULONG viewportwidth = qbinfo.viewportStopPixel - qbinfo.viewportStartPixel + 1;
    displayNode.TARGET_SIZE = (qbinfo.dst.height << 16) | (viewportwidth & 0xffff);

    if(qbinfo.isSrcMacroBlock)
    {
        /*
         * Base addresses for macroblock formats are easy, as field access
         * is taken care of by the hardware using appropriate y coordinates;
         * therefore all the addresses are the same.
         */
        displayNode.topField.LUMA_BA    = baseAddress;
        displayNode.botField.LUMA_BA    = baseAddress;
        displayNode.altTopField.LUMA_BA = baseAddress;
        displayNode.altBotField.LUMA_BA = baseAddress;

        displayNode.CHR_BA = baseAddress + pFrame->src.chromaBufferOffset;

        if(qbinfo.isSrc420)
        {
            DEBUGF2(3,("CGammaCompositorDISP::setMemoryAddressing 'surface format 420MB'\n"));
            displayNode.MA_CTL |= DISP_MA_CTL__FORMAT_YCBCR_420_MB;
        }
        else
        {
            DEBUGF2(3,("CGammaCompositorDISP::setMemoryAddressing 'surface format 422MB'\n"));
            displayNode.MA_CTL |= DISP_MA_CTL__FORMAT_YCBCR_422_MB;
        }

        /*
         * Pitch - given for luma in _pixels_, this must be rounded up to
         * a macroblock size (16 pixels).
         */
        displayNode.PMP = ((pFrame->src.ulStride + 15) & ~15);

        if(qbinfo.isSourceInterlaced)
        {
            displayNode.MA_CTL |= (DISP_MA_CTL__CHROMA_AS_FIELD |
                                   DISP_MA_CTL__LUMA_AS_FIELD);
        }

    }
    else
    {
        DEBUGF2(3,("CGammaCompositorDISP::setMemoryAddressing 'surface format 422R'\n"));

        displayNode.MA_CTL |= DISP_MA_CTL__FORMAT_YCBCR_422_R;

        if(qbinfo.isSourceInterlaced)
        {
            ULONG secondLineAddress = baseAddress+pFrame->src.ulStride;

            displayNode.topField.LUMA_BA    = baseAddress;
            displayNode.botField.LUMA_BA    = secondLineAddress;

            displayNode.altBotField.LUMA_BA = baseAddress;
            displayNode.altTopField.LUMA_BA = secondLineAddress;

            /*
             * To get field access we multiply the stride by two and move
             * the start address of the bottom node to the "next line"
             */
            displayNode.PMP = pFrame->src.ulStride*2;
        }
        else
        {
            /*
             * Frame access (i.e. progressive content)
             */
            displayNode.topField.LUMA_BA    = baseAddress;
            displayNode.botField.LUMA_BA    = baseAddress;
            displayNode.altTopField.LUMA_BA = baseAddress;
            displayNode.altBotField.LUMA_BA = baseAddress;

            displayNode.PMP = pFrame->src.ulStride;
        }
    }

}
