/***********************************************************************
 *
 * File: ./Gamma/GammaCompositorVideoPlane.cpp
 * Copyright (c) 2006 by STMicroelectronics. All rights reserved.
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

#include "GammaCompositorVideoPlane.h"

#include "GammaDisplayFilter.h"

/*
 * There are two sets of filter tables loaded into the video display
 * hardware, one for horizontal filtering and one for vertical. Each
 * table contain two sets of coefficients, the first set being for
 * the Luma and the second for Chroma.
 */
const int CGammaCompositorVideoPlane::HFILTER_ENTRIES = 35;
const int CGammaCompositorVideoPlane::HFILTER_SIZE = HFILTER_ENTRIES*sizeof(ULONG);
const int CGammaCompositorVideoPlane::VFILTER_ENTRIES = 22;
const int CGammaCompositorVideoPlane::VFILTER_SIZE = VFILTER_ENTRIES*sizeof(ULONG);

CGammaCompositorVideoPlane::CGammaCompositorVideoPlane(stm_plane_id_t   GDPid,
                                                       CGammaVideoPlug *videoPlug,
                                                       ULONG            baseAddress): CGammaCompositorPlane(GDPid)
{
    DEBUGF2(2,("CGammaCompositorVideoPlane::CGammaCompositorVideoPlane - in\n"));

    /*
     * Override the size of the node list for all video planes to prevent
     * LinuxDVB video buffer queuing ever failing.
     *
     * TODO: Reduce this back down to a more sane value once we have fixed
     *       the state management problems in the DEI and FlexVP for a
     *       queue fail/re-queue sequence.
     */
    m_ulNodeEntries = 128;

    m_baseAddress   = baseAddress;
    m_videoPlug     = videoPlug;

    m_fixedpointONE = 1<<13;              // n.13 fixed point for DISP rescaling
    m_ulMaxHSrcInc  = 4*m_fixedpointONE;  // downscale 4x
    m_ulMinHSrcInc  = m_fixedpointONE/32; // upscale 32x
    m_ulMaxVSrcInc  = 4*m_fixedpointONE;
    m_ulMinVSrcInc  = m_fixedpointONE/32;

    g_pIOS->ZeroMemory(&m_HFilter,sizeof(DMA_Area));
    g_pIOS->ZeroMemory(&m_VFilter,sizeof(DMA_Area));

    g_pIOS->ZeroMemory(&m_HVSRCState, sizeof(HVSRCState));

    /*
     * All of our video pipelines support non-linear zoom for 4:3 to 16:9
     * pixel aspect ratio conversion.
     */
    m_bHasNonLinearZoom = true;

    DEBUGF2(2,("CGammaCompositorVideoPlane::CGammaCompositorVideoPlane - out\n"));
}


bool CGammaCompositorVideoPlane::Create(void)
{
    if(!CDisplayPlane::Create())
        return false;

    /*
     * Each filter table contains _two_ complete sets of filters, one for
     * Luma and one for Chroma.
     */
    g_pIOS->AllocateDMAArea(&m_HFilter, HFILTER_SIZE*2, 16, SDAAF_NONE);
    if(!m_HFilter.pMemory)
    {
        DEBUGF2(1,("CGammaCompositorVideoPlane::Create 'out of memory'\n"));
        return false;
    }

    DEBUGF2(2,("CGammaCompositorVideoPlane::Create &m_HFilter = %p pMemory = %p pData = %p phys = %08lx\n",&m_HFilter,m_HFilter.pMemory,m_HFilter.pData,m_HFilter.ulPhysical));

    g_pIOS->AllocateDMAArea(&m_VFilter, VFILTER_SIZE*2, 16, SDAAF_NONE);
    if(!m_VFilter.pMemory)
    {
        DEBUGF2(1,("CGammaCompositorVideoPlane::Create 'out of memory'\n"));
        return false;
    }

    DEBUGF2(2,("CGammaCompositorVideoPlane::Create &m_VFilter = %p pMemory = %p pData = %p phys = %08lx\n",&m_VFilter,m_VFilter.pMemory,m_VFilter.pData,m_VFilter.ulPhysical));
    return true;
}


CGammaCompositorVideoPlane::~CGammaCompositorVideoPlane()
{
    g_pIOS->FreeDMAArea(&m_HFilter);
    g_pIOS->FreeDMAArea(&m_VFilter);
}


bool CGammaCompositorVideoPlane::SetControl(stm_plane_ctrl_t control, ULONG value)
{
    if(!m_videoPlug->SetControl(control,value))
        return CGammaCompositorPlane::SetControl(control,value);

    return true;
}


bool CGammaCompositorVideoPlane::GetControl(stm_plane_ctrl_t control, ULONG *value) const
{
    if(!m_videoPlug->GetControl(control,value))
        return CGammaCompositorPlane::GetControl(control,value);

    return true;
}


void CGammaCompositorVideoPlane::ResetQueueBufferState(void)
{
    DENTRY();

    CGammaCompositorPlane::ResetQueueBufferState();

    g_pIOS->ZeroMemory(&m_HVSRCState, sizeof(HVSRCState));

    DEXIT();
}

