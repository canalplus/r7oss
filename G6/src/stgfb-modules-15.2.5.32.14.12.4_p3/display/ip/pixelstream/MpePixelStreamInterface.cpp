/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2013-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "MpePixelStreamInterface.h"

///////////////////////////////////////////////////////////////////////////////
// Base class for CMpePixelStreamInterface interface
//

CMpePixelStreamInterface::CMpePixelStreamInterface ( uint32_t interfaceID, CDisplaySource * pSource, PixelStreamIrqInfo_t * IrqInfo, int32_t hwId, uint16_t sourceType, uint16_t sourceInstance)
    : CPixelStreamInterface(interfaceID, pSource)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRC( TRC_ID_MAIN_INFO, "Create CMpePixelStreamInterface %p with Id = %d", this, interfaceID );

    m_pixel_stream_params.source_type     = sourceType;
    m_pixel_stream_params.instance_number = sourceInstance;
    m_interfaceHwId                       = hwId;

    vibe_os_zero_memory(&m_display_buffer, sizeof(stm_display_buffer_t));

    m_pPixelStreamIrqInfo                 = IrqInfo;
    m_pDevRegs                            = 0;

    TRC( TRC_ID_MAIN_INFO, "Created CMpePixelStreamInterface = %p", this );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CMpePixelStreamInterface::~CMpePixelStreamInterface()
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRC( TRC_ID_MAIN_INFO, "CMpePixelStreamInterface %p Destroyed", this );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CMpePixelStreamInterface::Create(void)
{
    CDisplaySource       * pSource = GetParentSource();
    const CDisplayDevice * pDev    = pSource->GetParentDevice();
    uint32_t               size_pPlaneSetup;

    TRCIN( TRC_ID_MAIN_INFO, "" );

    if (!CPixelStreamInterface::Create ())
        return false;

    /*
       Allocate an array used to store the pointers
       to the PlaneSetup data specific to each plane using plane id
    */
    size_pPlaneSetup = pDev->GetNumberOfPlanes() * sizeof(void*);

    m_pDevRegs          = (uint32_t*)pDev->GetCtrlRegisterBase();

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;
}

void CMpePixelStreamInterface::SourceVSyncUpdateHW(bool isInterlaced,
                                                   bool isTopVSync,
                                                   const stm_time64_t &vsyncTime)
{
    CDisplaySource       * pSource = GetParentSource();
    const CDisplayDevice * pDev    = pSource->GetParentDevice();
    CDisplayPlane        * pConnectedPlane = 0;
    uint32_t               num_planes_Ids = 0;
    uint32_t               PlanesID[CINTERFACE_MAX_PLANES_PER_SOURCE] = {0};

    /* Get the connected planes ids */
    num_planes_Ids = pSource->GetConnectedPlaneID(PlanesID, N_ELEMENTS(PlanesID));

    /*
        apply plane set up if plane is connected to source
        and prepared plane setup exists
    */
    for(uint32_t i=0; (i<num_planes_Ids) ; i++)
    {
        pConnectedPlane = pDev->GetPlane(PlanesID[i]);
        if (pConnectedPlane)
        {
            pConnectedPlane->PresentDisplayNode( 0,             /* Previous node */
                                                &m_displayNode, /* Current node */
                                                 0,             /* Next node */
                                                 false,
                                                 false,         /* "isInterlaced" is not used by the FVDP */
                                                 false,         /* "isTopFieldOnDisplay" is not used by the FVDP */
                                                 vsyncTime);
        }
    }
    return;
}

bool CMpePixelStreamInterface::IsConnectionPossible(CDisplayPlane *pPlane) const
{
    bool isAllowed;

    TRCIN( TRC_ID_MAIN_INFO, "" );

    if (pPlane->isVideoPlane())
    {
        isAllowed = true;
    }
    else
    {
        isAllowed = false;
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return isAllowed;
}

stm_display_timing_events_t CMpePixelStreamInterface::GetInterruptStatus(void)
{
    if(m_pPixelStreamIrqInfo)
    {
        /* Read the interrupt status register */
        uint32_t ulIntStatus  = ReadInterruptReg(m_pPixelStreamIrqInfo->StatusRegOffset);

        if (ulIntStatus & m_pPixelStreamIrqInfo->StatusMask)
        {
            SetInterruptReg(m_pPixelStreamIrqInfo->StatusRegOffset, m_pPixelStreamIrqInfo->StatusMask);
        }

        DASSERTF((ulIntStatus != 0),("CMpePixelStreamInterface::GetInterruptStatus: NULL interrupt status\n"), STM_TIMING_EVENT_NONE);

        TRC( TRC_ID_UNCLASSIFIED, "CMpePixelStreamInterface::GetInterruptStatus(this=%p): ulIntStatus = 0x%x", this, ulIntStatus );

        return STM_TIMING_EVENT_FRAME;
    }
    else
        return STM_TIMING_EVENT_NONE;
}

int CMpePixelStreamInterface::Release(stm_display_source_pixelstream_h p)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return false;
}

int CMpePixelStreamInterface::SetInputParams(stm_display_source_pixelstream_h p,
                                             const stm_display_source_pixelstream_params_t * params)
{
    CDisplaySource       * pSource = GetParentSource();
    const CDisplayDevice * pDev    = pSource->GetParentDevice();
    CDisplayPlane        * pConnectedPlane = 0;
    uint32_t               num_planes_Ids = 0;
    uint32_t               height_multi_factor = 1;
    uint32_t               PlanesID[CINTERFACE_MAX_PLANES_PER_SOURCE] = {0};

    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_display_buffer.src.flags = 0;

    if(params->flags & STM_PIXELSTREAM_SRC_INTERLACED)
    {
        m_display_buffer.src.flags |= STM_BUFFER_SRC_INTERLACED;
        height_multi_factor = 2;
    }
    /* translate pixel stream information into plane setup info */
    m_display_buffer.src.primary_picture.pitch = params->htotal;
    m_display_buffer.src.primary_picture.height = params->vtotal * height_multi_factor;
    m_display_buffer.src.primary_picture.pixel_depth = params->colordepth;

    switch (params->colorType)
    {
    case STM_PIXELSTREAM_SRC_RGB:
        m_display_buffer.src.primary_picture.color_fmt = SURF_RGB888;
        break;
    case STM_PIXELSTREAM_SRC_YUV_422:
        m_display_buffer.src.primary_picture.color_fmt = SURF_YCBCR422MB;
        break;
    case STM_PIXELSTREAM_SRC_YUV_444:
    default:
        m_display_buffer.src.primary_picture.color_fmt = SURF_NULL_PAD;
        break;
    }

    if(params->flags & STM_PIXELSTREAM_SRC_COLORSPACE_709)
    {
        m_display_buffer.src.flags |= STM_BUFFER_SRC_COLORSPACE_709;
    }

    m_display_buffer.src.visible_area.x = 0;
    m_display_buffer.src.visible_area.y = 0;

    /* Frame size */
    m_display_buffer.src.visible_area.width = params->active_window.width;
    m_display_buffer.src.visible_area.height = params->active_window.height * height_multi_factor;

    m_display_buffer.src.pixel_aspect_ratio.numerator = params->pixel_aspect_ratio.numerator;
    m_display_buffer.src.pixel_aspect_ratio.denominator = params->pixel_aspect_ratio.denominator;

    m_display_buffer.src.linear_center_percentage = 100;
    m_display_buffer.src.ulConstAlpha = 255;

    m_display_buffer.src.ColorKey.flags = SCKCF_NONE;
    m_display_buffer.src.ColorKey.enable = '\0';
    m_display_buffer.src.ColorKey.format = SCKCVF_RGB;
    m_display_buffer.src.ColorKey.r_info = SCKCCM_DISABLED;
    m_display_buffer.src.ColorKey.g_info = SCKCCM_DISABLED;
    m_display_buffer.src.ColorKey.b_info = SCKCCM_DISABLED;
    m_display_buffer.src.ColorKey.minval = 0;
    m_display_buffer.src.ColorKey.maxval = 0;

    m_display_buffer.src.clut_bus_address = 0;
    m_display_buffer.src.post_process_luma_type = 0;
    m_display_buffer.src.post_process_chroma_type = 0;

    m_display_buffer.src.src_frame_rate.numerator = (int32_t)params->src_frame_rate;
    /* frame rate denominator ? */
    m_display_buffer.src.src_frame_rate.denominator = height_multi_factor*1000;

    m_display_buffer.src.config_3D.formats=params->config_3D.formats;
    switch (params->config_3D.formats)
    {
    case FORMAT_3D_SBS_HALF:
        m_display_buffer.src.config_3D.parameters.sbs.is_left_right_format=params->config_3D.parameters.sbs.is_left_right_format;
        m_display_buffer.src.config_3D.parameters.sbs.sbs_sampling_mode=params->config_3D.parameters.sbs.sbs_sampling_mode;
        break;
    case FORMAT_3D_STACKED_HALF:
        m_display_buffer.src.config_3D.parameters.frame_packed.is_left_right_format=params->config_3D.parameters.frame_packed.is_left_right_format;
        break;
    case FORMAT_3D_FRAME_SEQ:
        break;
    case FORMAT_3D_NONE:
    case FORMAT_3D_STACKED_FRAME:
    case FORMAT_3D_SBS_FULL:
    case FORMAT_3D_FIELD_ALTERNATE:
    case FORMAT_3D_PICTURE_INTERLEAVE:
    case FORMAT_3D_L_D:
    case FORMAT_3D_L_D_G_GMINUSD:
    default:
        TRC( TRC_ID_ERROR, "3Dmode not supported" );
        break;
    }

    m_displayNode.m_pictureId = 0;       // Not used
    m_displayNode.m_isPictureValid = true;
    m_displayNode.m_bufferDesc = m_display_buffer;

    // OLO_TO_DO : What field type shall we provide???
    if(params->flags & STM_PIXELSTREAM_SRC_INTERLACED)
    {
        m_displayNode.m_srcPictureType = GNODE_TOP_FIELD;
        m_displayNode.m_srcPictureTypeChar = 'T';
    }
    else
    {
        m_displayNode.m_srcPictureType = GNODE_PROGRESSIVE;
        m_displayNode.m_srcPictureTypeChar = 'F';
    }

    /* Get the connected planes ids */
    num_planes_Ids = pSource->GetConnectedPlaneID(PlanesID, N_ELEMENTS(PlanesID));

    for(uint32_t i=0; (i<num_planes_Ids) ; i++)
    {
        pConnectedPlane = pDev->GetPlane(PlanesID[i]);
        if (pConnectedPlane)
        {
            pConnectedPlane->SetCompoundControl(PLANE_CTRL_INPUT_WINDOW_VALUE, (void *) &(params->active_window));
        }
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;
}


int32_t CMpePixelStreamInterface::ConnectSource(CDisplayPlane *pPlane)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return -1;
}

bool CMpePixelStreamInterface::DisconnectSource(CDisplayPlane *pPlane)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;
}

bool CMpePixelStreamInterface::Flush(bool bFlushCurrentNode,
                                     const void * const user,
                                     bool bCheckUser)
{
    bool                   res = true;
    uint32_t               num_planes_Ids = 0;
    uint32_t               PlanesID[CINTERFACE_MAX_PLANES_PER_SOURCE];
    CDisplaySource       * pDS  = GetParentSource();
    const CDisplayDevice * pDev = pDS->GetParentDevice();

    TRCIN( TRC_ID_MAIN_INFO, "" );
    // Get the connected planes ids
    num_planes_Ids = pDS->GetConnectedPlaneID(PlanesID, N_ELEMENTS(PlanesID));
    // Call Plane's Flush method
    for(uint32_t i=0; (i<num_planes_Ids) ; i++)
    {
        CDisplayPlane* pDP  = pDev->GetPlane(PlanesID[i]);
        if (pDP)
        {
            res = pDP->Flush(bFlushCurrentNode, false, 0);
            if (!res)
            {
                TRC( TRC_ID_ERROR, "failed to flush the Plane = %p'", pDP );
                break;
            }
        }
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return res;
}

void CMpePixelStreamInterface::EnableInterrupts(void)
{
    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if(m_pPixelStreamIrqInfo)
    {
        // Clear Interrupts Status
        SetInterruptReg(m_pPixelStreamIrqInfo->StatusRegOffset, m_pPixelStreamIrqInfo->StatusMask);
        // Enable interrupts
        SetInterruptReg(m_pPixelStreamIrqInfo->EnableRegOffset, m_pPixelStreamIrqInfo->EnableMask);
    }

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}

void CMpePixelStreamInterface::DisableInterrupts(void)
{
    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if(m_pPixelStreamIrqInfo)
    {
        // Disable interrupts
        ClearInterruptReg(m_pPixelStreamIrqInfo->EnableRegOffset, m_pPixelStreamIrqInfo->EnableMask);
    }

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}

bool CMpePixelStreamInterface::SetInterfaceStatus(SourceInterfaceStatus status)
{
  bool Result = true;

  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(status)
  {
    case STM_SRC_INTERFACE_NOT_USED:
    case STM_SRC_INTERFACE_UNSTABLE:
      /* Update parent class status before going on */
      Result = CSourceInterface::SetInterfaceStatus(status);
      /* Disable IT generation */
      if(Result)
        DisableInterrupts();
      break;
    case STM_SRC_INTERFACE_STABLE:
      /* Enable IT generation */
      EnableInterrupts();

      /* fall through */
    case STM_SRC_INTERFACE_IN_USE:
      /* Update parent class status before leaving */
      Result = CSourceInterface::SetInterfaceStatus(status);
      break;
    default:
      Result = false;
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return Result;
}

void CMpePixelStreamInterface::Suspend(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  SetInterfaceStatus(STM_SRC_INTERFACE_UNSTABLE);

  CSourceInterface::Suspend();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CMpePixelStreamInterface::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  CSourceInterface::Resume();

  SetInterfaceStatus(STM_SRC_INTERFACE_STABLE);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
