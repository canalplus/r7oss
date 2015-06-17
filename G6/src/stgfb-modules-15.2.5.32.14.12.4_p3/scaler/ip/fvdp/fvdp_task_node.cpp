/***********************************************************************
 *
 * File: scaler/ip/fvdp/fvdp_task_node.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include "fvdp_scaler.h"
#include "fvdp.h"
#include "fvdp_task_node.h"

bool CFvdpTaskNode::PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr)
{
    area_config_t area_config;

    // Call subclass initialization
    if (!CScalingTaskNode::PrepareAndCheckScaling(task_descr, &area_config))
    {
        return false;
    }

    if(!PrepareAndCheckScalerFlags(task_descr))
    {
        return false;
    }

    if(!PrepareAndCheckInputVideoInfo(task_descr))
    {
        return false;
    }

    if(!PrepareAndCheckInputCroppingWindow(task_descr, &area_config))
    {
        return false;
    }

    if(!PrepareAndCheckOutputVideoInfo(task_descr))
    {
        return false;
    }

    if(!PrepareAndCheckOutputActiveWindow(task_descr, &area_config))
    {
        return false;
    }

    return true;
}

bool CFvdpTaskNode::PrepareAndCheckScalerFlags(const stm_scaler_task_descr_t *task_descr)
{
    if(task_descr->scaler_flags & STM_SCALER_FLAG_TNR)
    {
        m_TnrState = FVDP_PSI_ON;
    }
    else
    {
        m_TnrState = FVDP_PSI_OFF;
    }

    return true;
}

bool CFvdpTaskNode::PrepareAndCheckInputVideoInfo(const stm_scaler_task_descr_t *task_descr)
{
    m_InputVideoInfo.FrameID                  = m_FrameId;

    if(task_descr->input.buffer_flags & STM_SCALER_BUFFER_INTERLACED)
    {
        m_InputVideoInfo.ScanType             = INTERLACED;
        m_InputVideoInfo.Height               = (task_descr->input.height) / 2;
        if(task_descr->input.buffer_flags & STM_SCALER_BUFFER_TOP)
        {
            m_InputVideoInfo.FieldType        = TOP_FIELD;
        }
        else
        {
            m_InputVideoInfo.FieldType        = BOTTOM_FIELD;
        }
    }
    else
    {
        m_InputVideoInfo.ScanType             = PROGRESSIVE;
        m_InputVideoInfo.FieldType            = AUTO_FIELD_TYPE; // parameter initialized but normally not used
        m_InputVideoInfo.Height               = task_descr->input.height;
    }

    if((task_descr->input.color_space) & (STM_SCALER_COLORSPACE_RGB | STM_SCALER_COLORSPACE_RGB_VIDEORANGE))
    {
        m_InputVideoInfo.ColorSpace           = RGB;
    }
    else if((task_descr->input.color_space) & (STM_SCALER_COLORSPACE_BT601 | STM_SCALER_COLORSPACE_BT601_FULLRANGE))
    {
        m_InputVideoInfo.ColorSpace           = sdYUV;
    }
    else if((task_descr->input.color_space) & (STM_SCALER_COLORSPACE_BT709 | STM_SCALER_COLORSPACE_BT709_FULLRANGE))
    {
        m_InputVideoInfo.ColorSpace           = hdYUV;
    }
    else
    {
        TRC( TRC_ID_ERROR, "Not supported input buffer color space" );
        return false;
    }

    switch(task_descr->input.format)
    {
        case STM_SCALER_BUFFER_RGB888:
            m_InputVideoInfo.ColorSampling    = FVDP_444;
            break;
        case STM_SCALER_BUFFER_YUV420_NV12:
            m_InputVideoInfo.ColorSampling    = FVDP_420;
            break;
        case STM_SCALER_BUFFER_YUV422_NV16:
            m_InputVideoInfo.ColorSampling    = FVDP_422;
            break;
        default:
            TRC( TRC_ID_ERROR, "Not supported input buffer format" );
            return false;
    }

    m_InputVideoInfo.FrameRate                = 0;                    // Not taken into account by FVDP ENCODER
    m_InputVideoInfo.Width                    = task_descr->input.width;
    m_InputVideoInfo.Stride                   = task_descr->input.stride;
    m_InputVideoInfo.AspectRatio              = FVDP_ASPECT_RATIO_UNKNOWN;
    m_InputVideoInfo.FVDP3DFormat             = FVDP_2D;
    m_InputVideoInfo.FVDP3DFlag               = FVDP_AUTO3DFLAG;
    m_InputVideoInfo.FVDP3DSubsampleMode      = FVDP_3D_NO_SUBSAMPLE;
    m_InputVideoInfo.FVDPDisplayModeFlag      = FVDP_FLAGS_NONE;      // Not taken into account by FVDP ENCODER

    m_InputLumaAddr     = task_descr->input.luma_address;
    m_InputChromaOffset = task_descr->input.chroma_offset;

    return true;
}

bool CFvdpTaskNode::PrepareAndCheckInputCroppingWindow(const stm_scaler_task_descr_t *task_descr,
                                                       area_config_t                 *area_config)
{
    int32_t             y_crop;
    uint32_t            height_crop;

    m_InputCropWindow.HCropStart       = area_config->input_crop_area.x;
    m_InputCropWindow.HCropStartOffset = area_config->input_crop_area.x % 32;
    m_InputCropWindow.HCropWidth       = area_config->input_crop_area.width;

    if(task_descr->input.buffer_flags & STM_SCALER_BUFFER_INTERLACED)
    {
        y_crop                             = (area_config->input_crop_area.y) / 2;
        height_crop                        = (area_config->input_crop_area.height) / 2;
    }
    else
    {
        y_crop                             = (area_config->input_crop_area.y);
        height_crop                        = (area_config->input_crop_area.height);
    }
    m_InputCropWindow.VCropStart       = y_crop;
    m_InputCropWindow.VCropStartOffset = y_crop % 32;
    m_InputCropWindow.VCropHeight      = height_crop;

    // Adjust input crop area to hardware contraints
    if(!AdjustInputCropWindowForHWConstraints())
    {
        return false;
    }

    return true;
}

bool CFvdpTaskNode::AdjustInputCropWindowForHWConstraints(void)
{
    // Hardware contraints: x should be even and width should be a multiple of 4.
    m_InputCropWindow.HCropStart &= ~(0x1);
    m_InputCropWindow.HCropWidth = DIV_ROUNDED_DOWN(m_InputCropWindow.HCropWidth,4) * 4;

    // Check that input crop window is not null
    if(m_InputCropWindow.HCropWidth  == 0 ||
       m_InputCropWindow.VCropHeight == 0 )
    {
        return false;
    }

    return true;
}


bool CFvdpTaskNode::PrepareAndCheckOutputVideoInfo(const stm_scaler_task_descr_t *task_descr)
{
    m_OutputVideoInfo.FrameID                  = m_FrameId;

    if(task_descr->output.buffer_flags & STM_SCALER_BUFFER_INTERLACED)
    {
        m_OutputVideoInfo.ScanType             = INTERLACED;
        m_OutputVideoInfo.Height               = (task_descr->output.height) / 2;
    }
    else
    {
        m_OutputVideoInfo.ScanType             = PROGRESSIVE;
        m_OutputVideoInfo.Height               = task_descr->output.height;
    }
    m_OutputVideoInfo.FieldType = AUTO_FIELD_TYPE;

    if((task_descr->output.color_space) & (STM_SCALER_COLORSPACE_RGB | STM_SCALER_COLORSPACE_RGB_VIDEORANGE))
    {
        m_OutputVideoInfo.ColorSpace           = RGB;
    }
    else if((task_descr->output.color_space) & (STM_SCALER_COLORSPACE_BT601 | STM_SCALER_COLORSPACE_BT601_FULLRANGE))
    {
        m_OutputVideoInfo.ColorSpace           = sdYUV;
    }
    else if((task_descr->output.color_space) & (STM_SCALER_COLORSPACE_BT709 | STM_SCALER_COLORSPACE_BT709_FULLRANGE))
    {
        m_OutputVideoInfo.ColorSpace           = hdYUV;
    }
    else
    {
        TRC( TRC_ID_ERROR, "Not supported output buffer color space" );
        return false;
    }

    switch(task_descr->output.format)
    {
        case STM_SCALER_BUFFER_RGB888:
            m_OutputVideoInfo.ColorSampling    = FVDP_444;
            break;
        case STM_SCALER_BUFFER_YUV420_NV12:
            m_OutputVideoInfo.ColorSampling    = FVDP_420;
            break;
        case STM_SCALER_BUFFER_YUV422_NV16:
            m_OutputVideoInfo.ColorSampling    = FVDP_422;
            break;
        default:
            TRC( TRC_ID_ERROR, "Not supported output buffer format" );
            return false;
    }

    m_OutputVideoInfo.FrameRate                = 0;                    // Cannot guest this info, not inside the scaler API
    m_OutputVideoInfo.Width                    = task_descr->output.width;
    m_OutputVideoInfo.Stride                   = task_descr->output.stride;
    m_OutputVideoInfo.AspectRatio              = FVDP_ASPECT_RATIO_UNKNOWN;
    m_OutputVideoInfo.FVDP3DFormat             = FVDP_2D;
    m_OutputVideoInfo.FVDP3DFlag               = FVDP_AUTO3DFLAG;
    m_OutputVideoInfo.FVDP3DSubsampleMode      = FVDP_3D_NO_SUBSAMPLE;
    m_OutputVideoInfo.FVDPDisplayModeFlag      = FVDP_FLAGS_NONE;      // Perhaps flag REPEATED to set?

    m_OutputLumaAddr     = task_descr->output.luma_address;
    m_OutputChromaOffset = task_descr->output.chroma_offset;

    return true;
}

bool CFvdpTaskNode::PrepareAndCheckOutputActiveWindow(const stm_scaler_task_descr_t *task_descr,
                                                      area_config_t                 *area_config)
{
    m_OutputActiveWindow.HStart  = area_config->output_active_area.x;
    m_OutputActiveWindow.HWidth  = area_config->output_active_area.width;

    if(task_descr->output.buffer_flags & STM_SCALER_BUFFER_INTERLACED)
    {
        m_OutputActiveWindow.VStart  = area_config->output_active_area.y / 2;
        m_OutputActiveWindow.VHeight = area_config->output_active_area.height / 2;
    }
    else
    {
        m_OutputActiveWindow.VStart  = area_config->output_active_area.y;
        m_OutputActiveWindow.VHeight = area_config->output_active_area.height;
    }

    // Adjust output active area to hardware contraints
    if(!AdjustOutputActiveWindowForHWConstraints())
    {
        return false;
    }

    return true;
}

bool CFvdpTaskNode::AdjustOutputActiveWindowForHWConstraints(void)
{
    // Hardware contraint: width cannot be odd. Truncate to even value below.
    m_OutputActiveWindow.HWidth &= ~(0x1);

    // Check that output active window is not null
    if(m_OutputActiveWindow.HWidth  == 0 ||
       m_OutputActiveWindow.VHeight == 0 )
    {
        return false;
    }

    return true;
}

