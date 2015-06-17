/***********************************************************************
 *
 * File: scaler/ip/blitter/blitter_task_node.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include "blitter_scaler.h"

bool CBlitterTaskNode::PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr)
{
    area_config_t area_config;

    // Call subclass initialization
    if (!CScalingTaskNode::PrepareAndCheckScaling(task_descr, &area_config))
    {
        return false;
    }

    // check scaler flags
    if(!PrepareAndCheckScalerFlags(task_descr))
    {
        return false;
    }

    // prepare the source surface
    if (!PrepareSourceSurface(task_descr, &area_config))
    {
        return false;
    }

    // prepare the destination surface
    if (!PrepareDestinationSurface(task_descr, &area_config))
    {
        return false;
    }

    return true;
}

bool CBlitterTaskNode::PrepareAndCheckScalerFlags(const stm_scaler_task_descr_t *task_descr)
{
    // TNR is not supported by blitter
    if(task_descr->scaler_flags & STM_SCALER_FLAG_TNR)
    {
        TRC( TRC_ID_ERROR, "TNR is not supported by blitter" );
        return false;
    }

    return true;
}

bool CBlitterTaskNode::PrepareSourceSurface(const stm_scaler_task_descr_t *task_descr,
                                            area_config_t                 *area_config)
{
    // Convert the source origin into something useful to the blitter
    m_blitterSrce.crop_rect.position.x    = area_config->input_crop_area.x;

    m_blitterSrce.crop_rect.size.w        = area_config->input_crop_area.width;

    m_blitterSrce.buffer_dimension.w      = task_descr->input.stride;

    m_blitterSrce.buffer_size             = task_descr->input.size;

    if((task_descr->input.buffer_flags) & STM_SCALER_BUFFER_INTERLACED)
    {
        // INTERLACED SOURCE
        m_blitterSrce.crop_rect.position.y    = area_config->input_crop_area.y / 2;
        m_blitterSrce.crop_rect.size.h        = area_config->input_crop_area.height / 2;
        m_blitterSrce.buffer_dimension.h      = task_descr->input.height / 2;
        m_blitterSrce.pitch                   = task_descr->input.stride * 2;

        if((task_descr->input.buffer_flags) & STM_SCALER_BUFFER_TOP)
        {
            // STM_SCALER_BUFFER_TOP
            m_blitterFlags                        = STM_BLITTER_SBF_DEINTERLACE_TOP;

            m_blitterSrce.buffer_add.base         = (unsigned long) task_descr->input.luma_address;
        }
        else
        {
            // STM_SCALER_BUFFER_BOTTOM
            m_blitterFlags                        = STM_BLITTER_SBF_DEINTERLACE_BOTTOM;

            m_blitterSrce.buffer_add.base         = ((unsigned long) task_descr->input.luma_address) + task_descr->input.stride;
        }
    }
    else
    {
        // PROGRESSIVE SOURCE
        m_blitterSrce.crop_rect.position.y    = area_config->input_crop_area.y;
        m_blitterSrce.crop_rect.size.h        = area_config->input_crop_area.height;
        m_blitterSrce.buffer_dimension.h      = task_descr->input.height;
        m_blitterSrce.pitch                   = task_descr->input.stride;

        m_blitterFlags                        = STM_BLITTER_SBF_NONE;

        m_blitterSrce.buffer_add.base         = (unsigned long) task_descr->input.luma_address;
    }

    m_blitterSrce.buffer_add.cbcr_offset  = task_descr->input.chroma_offset;

    switch(task_descr->input.format)
    {
        case STM_SCALER_BUFFER_YUV420_NV12:
            m_blitterSrce.format          = STM_BLITTER_SF_NV12;
            break;
        default:
            TRC( TRC_ID_ERROR, "Not supported input buffer format" );
            return false;
    }

    if (m_blitterSrce.format & STM_BLITTER_SF_YCBCR)
    {
        m_blitterSrce.cspace              = ((task_descr->input.color_space & STM_SCALER_COLORSPACE_BT709) ? STM_BLITTER_SCS_BT709 : STM_BLITTER_SCS_BT601);
    }
    else
    {
        m_blitterSrce.cspace              = STM_BLITTER_SCS_RGB;
    }

    return true;
}

bool CBlitterTaskNode::PrepareDestinationSurface(const stm_scaler_task_descr_t *task_descr, area_config_t *area_config)
{
    switch(task_descr->output.format)
    {
        case STM_SCALER_BUFFER_YUV420_NV12:
            m_blitterDest.format          = STM_BLITTER_SF_NV12;
            break;
        default:
            TRC( TRC_ID_ERROR, "Not supported output buffer format" );
            return false;
    }

    if (m_blitterDest.format & STM_BLITTER_SF_YCBCR)
    {
        m_blitterDest.cspace              = ((task_descr->output.color_space & STM_SCALER_COLORSPACE_BT709) ? STM_BLITTER_SCS_BT709 : STM_BLITTER_SCS_BT601);
    }
    else
    {
        m_blitterDest.cspace              = STM_BLITTER_SCS_RGB;
    }

    // Convert the destination origin into something useful to the blitter
    m_blitterDest.crop_rect.position.x         = area_config->output_active_area.x;
    m_blitterDest.crop_rect.position.y         = area_config->output_active_area.y;
    m_blitterDest.crop_rect.size.w             = area_config->output_active_area.width;

    m_blitterDest.buffer_dimension.w      = task_descr->output.stride;

    m_blitterDest.buffer_size             = task_descr->output.size;

    if((task_descr->output.buffer_flags) & STM_SCALER_BUFFER_INTERLACED)
    {
        // INTERLACED SOURCE
        m_blitterDest.crop_rect.size.h        = area_config->output_active_area.height / 2;
        m_blitterDest.buffer_dimension.h      = task_descr->output.height / 2;
        m_blitterDest.pitch                   = task_descr->output.stride * 2;

        // Reset blitter flags: because for P to I conversion or I to I conversion,
        // it is like a P to P conversion for the blitter. This flag should be equal
        // to STM_BLITTER_SBF_DEINTERLACE_TOP or STM_BLITTER_SBF_DEINTERLACE_BOTTOM
        // only in case of I to P conversion (for introducing phase shifting when
        // source is a bottom field).
        m_blitterFlags                        = STM_BLITTER_SBF_NONE;

        if((task_descr->output.buffer_flags) & STM_SCALER_BUFFER_TOP)
        {
            // STM_SCALER_BUFFER_TOP
            m_blitterDest.buffer_add.base         = (unsigned long) task_descr->output.luma_address;
        }
        else
        {
            // STM_SCALER_BUFFER_BOTTOM
            m_blitterDest.buffer_add.base         = ((unsigned long) task_descr->output.luma_address) + task_descr->output.stride;
        }
    }
    else
    {
        // PROGRESSIVE SOURCE
        m_blitterDest.crop_rect.size.h        = area_config->output_active_area.height;
        m_blitterDest.buffer_dimension.h      = task_descr->output.height;
        m_blitterDest.pitch                   = task_descr->output.stride;

        m_blitterDest.buffer_add.base         = (unsigned long) task_descr->output.luma_address;
    }

    m_blitterDest.buffer_add.cbcr_offset  = task_descr->output.chroma_offset;

    return true;
}

