/***********************************************************************
 *
 * File: scaler/generic/scaling_task_node.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>

#include "scaling_task_node.h"


bool CScalingTaskNode::PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr,
                                              area_config_t                 *area_config)
{
    // Save datas associated to client callbacks
    m_inputUserData  = task_descr->input.user_data;
    m_outputUserData = task_descr->output.user_data;

    // Fill area configuration with user info
    area_config->input_crop_area    = task_descr->input_crop_area;
    area_config->output_active_area = task_descr->output_image_area;

    // Truncate input crop area to picture buffer
    if(!TruncateAreaToLimits(&(area_config->input_crop_area), task_descr->input.width, task_descr->input.height))
    {
        TRC( TRC_ID_ERROR, "Failed, input crop area outside of picture buffer" );
        return false;
    }
    // Truncate output active area to picture buffer
    if(!TruncateAreaToLimits(&(area_config->output_active_area), task_descr->output.width, task_descr->output.height))
    {
        TRC( TRC_ID_ERROR, "Failed, output active area outside of picture buffer" );
        return false;
    }

    return true;
}

bool CScalingTaskNode::IsRectFullyOutOfBonds(stm_scaler_rect_t *rect_area, uint32_t buffer_width, uint32_t buffer_height)
{
    if( (rect_area->x + (int32_t)rect_area->width  <= 0) ||
        (rect_area->y + (int32_t)rect_area->height <= 0) ||
        (rect_area->x >= (int32_t)buffer_width)   ||
        (rect_area->y >= (int32_t)buffer_height)
      )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool CScalingTaskNode::TruncateAreaToLimits(stm_scaler_rect_t *rect_area, uint32_t buffer_width, uint32_t buffer_height)
{
    /* Check if rectangle area is outside of buffer size */
    if(IsRectFullyOutOfBonds(rect_area, buffer_width, buffer_height))
    {
        return false;
    }

    /* Truncate rectangle area to buffer size */
    if(rect_area->x < 0)
    {
        rect_area->width += rect_area->x;
        rect_area->x = 0;
    }
    if(rect_area->y < 0)
    {
        rect_area->height += rect_area->y;
        rect_area->y = 0;
    }
    if(rect_area->x + (int32_t)rect_area->width > (int32_t)buffer_width)
    {
        rect_area->width = buffer_width - rect_area->x;
    }
    if(rect_area->y + (int32_t)rect_area->height > (int32_t)buffer_height)
    {
        rect_area->height = buffer_height - rect_area->y;
    }

    return true;
}
