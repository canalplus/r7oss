/***********************************************************************
 *
 * File: scaler/generic/scaling_task_node.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALING_TASK_NODE_H
#define SCALING_TASK_NODE_H

#ifdef __cplusplus

#include <stm_scaler.h>
#include "scaling_node.h"

class CScalingCtxNode;
class CScalingTaskNode;
class CScaler;

typedef struct
{
    CScaler             *scaler_object;     // pointer on scaler object
    bool                 aborted;           // Check callback msg validity, scaling tasks could be deleted by Abort before callback is executed
    CScalingTaskNode    *scaling_task_node; // pointer on scaling task node
} callback_msg_t;

typedef struct
{
    stm_scaler_rect_t    input_crop_area;
    stm_scaler_rect_t    output_active_area;
} area_config_t;

class CScalingTaskNode : public CScalingNode
{
public:
    CScalingTaskNode() : CScalingNode(TYPE_SCALING_TASK)
      {
        m_scalingCtxNode = 0;
        m_FrameId = 0;
        m_scalingTaskStatus = 0;
        m_isScalingTaskContentValid = false;
        m_allocatedScalingTaskDoneMsg = 0;
        m_allocatedScalingBufferDoneMsg = 0;
        m_inputUserData = 0;
        m_outputUserData = 0;
      }

    virtual bool             PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr) = 0;

    CScalingCtxNode         *m_scalingCtxNode;                // scaling context associated to this scaling task
    uint32_t                 m_FrameId;                       // frame identifier
    uint32_t                 m_scalingTaskStatus;             // bit field of scaling_task_status_t
    bool                     m_isScalingTaskContentValid;     // set by callback under interrupt context, read under task context: no need of protection
    callback_msg_t          *m_allocatedScalingTaskDoneMsg;   // pre-allocation of scaling task done message that could be used under interrupt context
    callback_msg_t          *m_allocatedScalingBufferDoneMsg; // pre-allocation of scaling buffer done message that could be used under interrupt context

    void                    *m_inputUserData;                 // client user data for input buffer coming from task_descr
    void                    *m_outputUserData;                // client user data for output buffer coming from task_descr

protected:
    virtual bool             PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr, area_config_t *area_config);

    bool                     IsRectFullyOutOfBonds(stm_scaler_rect_t *rect_area, uint32_t buffer_width, uint32_t buffer_height);
    bool                     TruncateAreaToLimits(stm_scaler_rect_t *rect_area, uint32_t buffer_width, uint32_t buffer_height);

}; /* CScalingTaskNode */

#endif /* __cplusplus */

#endif /* SCALING_TASK_NODE_H */

