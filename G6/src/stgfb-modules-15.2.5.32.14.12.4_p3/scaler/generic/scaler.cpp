/***********************************************************************
 *
 * File: scaler/generic/scaler.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>
#include <stm_scaler.h>
#include <scaler_device_priv.h>

#include "scaler_device.h"
#include "scaler.h"
#include "scaler_handle.h"

// ---------------------------------------------------------------------------
// Functions called in workqueue context by worker thread
void scaling_task_done_cb_treatment(void *data)
{
    callback_msg_t    *callback_msg      = (callback_msg_t *)data;

    callback_msg->scaler_object->ScalingTaskDoneCbTreatment(data);

    vibe_os_delete_message(data);
}

void scaling_buffer_done_cb_treatment(void *data)
{
    callback_msg_t    *callback_msg      = (callback_msg_t *)data;

    callback_msg->scaler_object->ScalingBufferDoneCbTreatment(data);

    vibe_os_delete_message(data);
}

void process_next_scaling_task(void *data)
{
    process_next_scaling_task_msg_t *process_next_scaling_task_msg = (process_next_scaling_task_msg_t *)data;

    process_next_scaling_task_msg->scaler_object->ProcessNextScalingTaskNode();

    vibe_os_delete_message(data);
}
// ---------------------------------------------------------------------------

CScaler::CScaler(uint32_t id, uint32_t caps, const CScalerDevice *pDev)
{
    m_scalerId              = id;
    m_scalerCaps            = caps;
    m_pScalerDevice         = pDev;

    m_stopDispatchEvent     = false;

    m_pMessageThread        = 0;
    m_sysProtect            = 0;
    m_abortingSem           = 0;

    m_pCtxQueue             = 0;
    m_pScalingTaskNodePool  = 0;
    m_pPendingQueue         = 0;
    m_pRunningQueue         = 0;

    vibe_os_zero_memory(&m_Statistics, sizeof(m_Statistics));
}

CScaler::~CScaler()
{
    vibe_os_destroy_message_thread(m_pMessageThread);

    vibe_os_delete_semaphore(m_sysProtect);
    vibe_os_delete_semaphore(m_abortingSem);

    delete(m_pCtxQueue);
    delete(m_pPendingQueue);
    delete(m_pRunningQueue);
}

bool CScaler::Create(void)
{
    char message_thread_name[12+2];

    TRCIN( TRC_ID_MAIN_INFO, "" );

    // Create workqueue with its own single thread
    vibe_os_snprintf(message_thread_name, sizeof(message_thread_name), "VIB-Scaler%d", GetID());
    m_pMessageThread = vibe_os_create_message_thread(message_thread_name, thread_vib_scaler);
    if(!m_pMessageThread)
    {
        TRC( TRC_ID_ERROR, "Failed to create scaler message thread" );
        goto exit;
    }

    // Create system protection (initialize in free state to be used)
    m_sysProtect = vibe_os_create_semaphore(1);
    if( (m_sysProtect==0) )
    {
        TRC( TRC_ID_ERROR, "Failed to create m_sysProtect" );
        goto exit;
    }

    // Create m_abortingSem (initialize in lock state)
    m_abortingSem = vibe_os_create_semaphore(0);
    if(m_abortingSem==0)
    {
        TRC( TRC_ID_ERROR, "Failed to create m_abortingSem" );
        goto exit;
    }

    // Create the scaling context queue
    m_pCtxQueue = new CScalingQueue(TYPE_SCALING_CTX);

    // Create the queue of pending scaling tasks
    m_pPendingQueue  = new CScalingQueue(TYPE_SCALING_TASK);
    if(!m_pPendingQueue)
    {
        TRC( TRC_ID_ERROR, "Failed to create the queue of pending scaling tasks" );
        goto exit;
    }
    // Create the queue of running scaling tasks
    m_pRunningQueue  = new CScalingQueue(TYPE_SCALING_TASK);
    if(!m_pRunningQueue)
    {
        TRC( TRC_ID_ERROR, "Failed to create the queue of running scaling tasks" );
        goto exit;
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;

exit:

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return false;
}

scaler_error_t CScaler::Open(CScalingCtxNode * const scaling_ctx_node, const stm_scaler_config_t *scaler_config)
{
    scaler_error_t    error_code    = ERR_NONE;

    // Fill the scaling context
    scaling_ctx_node->m_config               = *scaler_config;
    scaling_ctx_node->m_aborting             = false;
    scaling_ctx_node->m_lastFrameId          = 0;
    scaling_ctx_node->m_nbScalingTaskRunning = 0;

    // Perform blitter or fvdp open (for fvdp, it should store the fvdp handle)
    error_code = PerformOpening( scaling_ctx_node );
    if(error_code != ERR_NONE)
    {
        TRC( TRC_ID_ERROR, "Failed to PerformOpening" );
        goto exit;
    }

    // Enqueue this new scaling context into the context queue
    if(!m_pCtxQueue->QueueScalingNode(scaling_ctx_node))
    {
        TRC( TRC_ID_ERROR, "Failed to enqueue this scaler context into context queue" );
        error_code = ERR_INTERNAL;
        goto exit_and_close;
    }

    return error_code;

// failing exit
exit_and_close:
    // Perform blitter or fvdp close
    if(PerformClosing(scaling_ctx_node) != ERR_NONE)
    {
        TRC( TRC_ID_ERROR, "Failed to PerformClosing of this scaler context" );
    }

exit:
    return error_code;
}

scaler_error_t CScaler::Close(CScalingCtxNode* const scaling_ctx_node)
{
    scaler_error_t   error_code = ERR_NONE;
    CScalingCtxNode *curr_scaling_ctx_node;

    TRC( TRC_ID_SCALER, "SCA CLOSE scaling_ctx_node=(%p)", scaling_ctx_node );

    error_code = Abort(scaling_ctx_node);
    if(error_code != ERR_NONE)
    {
        return error_code;
    }

    // Protected by Semaphore API against CScaler::Open access.

    // Parse the scaling context queue from the beginning
    curr_scaling_ctx_node = 0;
    while( (curr_scaling_ctx_node = (CScalingCtxNode *)m_pCtxQueue->GetNextScalingNode(curr_scaling_ctx_node)) )
    {
        // Check if it is the searched scaler context to delete
        if(curr_scaling_ctx_node == scaling_ctx_node)
        {
            // Remove this scaling context from scaling context queue
            if(!m_pCtxQueue->DequeueScalingNode(curr_scaling_ctx_node))
            {
                TRC( TRC_ID_ERROR, "Failed to remove this scaling context from scaling context queue" );
                error_code = ERR_INTERNAL;
            }

            if(PerformClosing(curr_scaling_ctx_node) != ERR_NONE)
            {
                TRC( TRC_ID_ERROR, "Failed to PerformClosing of this scaler context" );
                error_code = ERR_INTERNAL;
            }

            break;
        }
    }

    return error_code;
}

scaler_error_t CScaler::ProcessScalingTaskNode(CScalingTaskNode *scaling_task_node)
{
    scaler_error_t error_code = ERR_INTERNAL;

    // Dequeue scaling task node from pending queue
    if(!m_pPendingQueue->DequeueScalingNode(scaling_task_node))
    {
        TRC( TRC_ID_ERROR, "Failed to dequeue scaling task node from pending queue" );
        goto exit;
    }

    // Enqueue scaling task node to running queue
    if(!m_pRunningQueue->QueueScalingNode(scaling_task_node))
    {
        TRC( TRC_ID_ERROR, "Failed to enqueue scaling task node to running queue" );
        goto exit;
    }

    // Allocate callback messages in advance, because they could be executed from interrupt context
    scaling_task_node->m_allocatedScalingTaskDoneMsg   = (callback_msg_t *) vibe_os_allocate_message( scaling_task_done_cb_treatment, sizeof(callback_msg_t) );
    scaling_task_node->m_allocatedScalingBufferDoneMsg = (callback_msg_t *) vibe_os_allocate_message( scaling_buffer_done_cb_treatment, sizeof(callback_msg_t) );
    if(!(scaling_task_node->m_allocatedScalingTaskDoneMsg && scaling_task_node->m_allocatedScalingBufferDoneMsg))
    {
        TRC( TRC_ID_ERROR, "Failed, cannot allocate scaling task done message and scaling buffer done message" );
        goto exit_and_release;
    }

    // Fill callback messages
    scaling_task_node->m_allocatedScalingTaskDoneMsg->aborted             = false;
    scaling_task_node->m_allocatedScalingTaskDoneMsg->scaler_object       = this;
    scaling_task_node->m_allocatedScalingTaskDoneMsg->scaling_task_node   = scaling_task_node;
    scaling_task_node->m_allocatedScalingBufferDoneMsg->aborted           = false;
    scaling_task_node->m_allocatedScalingBufferDoneMsg->scaler_object     = this;
    scaling_task_node->m_allocatedScalingBufferDoneMsg->scaling_task_node = scaling_task_node;

    // In case of fvdp, this function start the scaling and return immediately.
    // In case of blitter, this function is blocking until the scaling is done.
    TRC( TRC_ID_SCALER, "SCA PROCESS SCALING TASK node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
    error_code = PerformScaling(scaling_task_node->m_scalingCtxNode, scaling_task_node);
    if(error_code == ERR_NONE)
    {
        scaling_task_node->m_scalingCtxNode->m_nbScalingTaskRunning++;
    }
    else
    {
        TRC( TRC_ID_ERROR, "Failed, to perform scaling" );
        goto exit_and_release;
    }

    return error_code;

exit_and_release:
    if(scaling_task_node->m_allocatedScalingTaskDoneMsg)
    {
        vibe_os_delete_message(scaling_task_node->m_allocatedScalingTaskDoneMsg);
        scaling_task_node->m_allocatedScalingTaskDoneMsg   = 0;
    }
    if(scaling_task_node->m_allocatedScalingBufferDoneMsg)
    {
        vibe_os_delete_message(scaling_task_node->m_allocatedScalingBufferDoneMsg);
        scaling_task_node->m_allocatedScalingBufferDoneMsg = 0;
    }

exit:
    return error_code;
}

void CScaler::ProcessNextScalingTaskNode(void)
{
    int               error_code = 0;
    CScalingTaskNode *curr_scaling_task_node;
    CScalingTaskNode *next_scaling_task_node;
    scaler_error_t    error;
    bool              can_start_new_scaling_task = true;

    error_code = vibe_os_down_semaphore(m_sysProtect);
    if(error_code)
    {
        TRC( TRC_ID_ERROR, "Failed, taking semaphore" );
    }

    // Parse the queue of pending scaling tasks from the beginning
    curr_scaling_task_node = (CScalingTaskNode*) m_pPendingQueue->GetNextScalingNode(0);
    while( curr_scaling_task_node )
    {
        error = ProcessScalingTaskNode(curr_scaling_task_node);
        if(error == ERR_NONE)
        {
            // Scaling started and on-going
            can_start_new_scaling_task = false;

            break;
        }
        else
        {
            // Problem, indicate to user by calling the callbacks
            TRC( TRC_ID_ERROR, "Failed to perform scaling (%d)", error );

            // Dequeue node and get next node because curr_node will be deleted after.
            next_scaling_task_node = (CScalingTaskNode*) m_pRunningQueue->GetNextScalingNode(curr_scaling_task_node);
            if(!m_pRunningQueue->DequeueScalingNode(curr_scaling_task_node))
            {
                TRC( TRC_ID_ERROR, "Failed to remove this node from m_pRunningQueue queue" );
            }

            // release this node and scaling task, call task done callback to inform user of the problem
            ReleasingScalingTaskNode(curr_scaling_task_node, true);

            // Continue with next node/scaling task
            curr_scaling_task_node = next_scaling_task_node;
        }
    }

    if(can_start_new_scaling_task)
    {
        TRC( TRC_ID_SCALER, "SCA WAIT DISPATCH EVT" );
        // No scaling task was restarted, so allow the Dispatch() function to send EVT_DISPATCH
        m_stopDispatchEvent = false;
    }

    vibe_os_up_semaphore(m_sysProtect);
}





void CScaler::ScalingTaskDoneCbTreatment(void *data)
{
    int               error_code        = 0;
    callback_msg_t   *callback_msg    = (callback_msg_t *)data;
    CScalingTaskNode *scaling_task_node;
    vibe_time64_t     time;

    error_code = vibe_os_down_semaphore(m_sysProtect);
    if(error_code)
    {
        TRC( TRC_ID_ERROR, "Failed, taking semaphore" );
    }

    if(!callback_msg->aborted)
    {
        scaling_task_node = callback_msg->scaling_task_node;

        if(!scaling_task_node->m_scalingCtxNode->m_aborting)
        {
            // Call scaling task done callback only if no abort on-going
            time = vibe_os_get_system_time();
            TRC( TRC_ID_SCALER, "SCA SCALING TASK DONE CB node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
            TRC( TRC_ID_API_SCALER, "SCA SCALING TASK DONE CB node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
            scaling_task_node->m_scalingCtxNode->m_config.scaler_task_done_callback(scaling_task_node->m_outputUserData,
                                                                                    time,
                                                                                    scaling_task_node->m_isScalingTaskContentValid);
            scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_DONE;

            // Update statistics
            if(scaling_task_node->m_isScalingTaskContentValid)
            {
                m_Statistics.PicTreatedValid++;
            }
            else
            {
                m_Statistics.PicTreatedInvalid++;
            }

        }
        // Call input buffer done callback in all cases to release input buffer.
        TRC( TRC_ID_SCALER, "SCA IN BUFF DONE CB node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
        scaling_task_node->m_scalingCtxNode->m_config.scaler_input_buffer_done_callback(scaling_task_node->m_inputUserData);
        scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_INPUT_BUFFER_NO_MORE_USED;
        m_Statistics.InputBufReleased++;
    }
    else
    {
        TRC( TRC_ID_SCALER, "Msg 0Aborted cbmsg=%p tn=%p fid=%u msgt=%u msgb=%u",
                callback_msg, callback_msg->scaling_task_node,
                callback_msg->scaling_task_node->m_FrameId,
                callback_msg->scaling_task_node->m_allocatedScalingTaskDoneMsg->aborted,
                callback_msg->scaling_task_node->m_allocatedScalingBufferDoneMsg->aborted );
    }

    vibe_os_up_semaphore(m_sysProtect);

    if(!callback_msg->aborted)
    {
        // Try to process next pending scaling task
        ProcessNextScalingTaskNode();
    }
    else
    {
        TRC( TRC_ID_SCALER, "Msg 1Aborted cbmsg=%p tn=%p fid=%u msgt=%u msgb=%u",
                callback_msg, callback_msg->scaling_task_node,
                callback_msg->scaling_task_node->m_FrameId,
                callback_msg->scaling_task_node->m_allocatedScalingTaskDoneMsg->aborted,
                callback_msg->scaling_task_node->m_allocatedScalingBufferDoneMsg->aborted );
    }
}


void CScaler::ScalingBufferDoneCbTreatment(void *data)
{
    int                error_code          = 0;
    callback_msg_t    *callback_msg        = (callback_msg_t *)data;
    CScalingTaskNode  *scaling_task_node;
    vibe_time64_t      time;

    error_code = vibe_os_down_semaphore(m_sysProtect);
    if(error_code)
    {
        TRC( TRC_ID_ERROR, "Failed, taking semaphore" );
    }

    if(!callback_msg->aborted)
    {
        scaling_task_node = callback_msg->scaling_task_node;

        TRC( TRC_ID_SCALER, "SCA OUT BUFF DONE CB node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );

        // Remove this scaling task from running queue
        if(!m_pRunningQueue->DequeueScalingNode(scaling_task_node))
        {
            TRC( TRC_ID_ERROR, "Failed to dequeue scaling task node from running queue" );
        }

        // Case where scaling task done callback was not called before scaling buffer done callback
        if(!(scaling_task_node->m_scalingTaskStatus & SCALING_TASK_DONE))
        {
            // If an abort is on-going, no need to call scaling task done callback because low layer don't call it too.
            if(!scaling_task_node->m_scalingCtxNode->m_aborting)
            {
                // But if no abort is on-going, it is mandatory to call scaling task done callback with "false" to indicate the problem to upper layer.
                TRC( TRC_ID_ERROR, "Failed, scaling task done callback not called %x", scaling_task_node->m_scalingTaskStatus );

                time = vibe_os_get_system_time();
                TRC( TRC_ID_API_SCALER, "SCA SCALING TD false CB node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
                scaling_task_node->m_scalingCtxNode->m_config.scaler_task_done_callback(scaling_task_node->m_outputUserData,
                                                                                    time,
                                                                                    false);
                scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_DONE;

                // Update statistics
                m_Statistics.PicTreatedInvalid++;

                // WHAT TO DO
                // Problem, should we start also a pending scaling task if any?
            }

            TRC( TRC_ID_SCALER, "SCA WAIT DISPATCH EVT" );
            // No scaling task on-going, so allow the Dispatch() function to send EVT_DISPATCH
            m_stopDispatchEvent = false;
        }

        // Case where scaling task done callback was not called before scaling buffer done callback
        if(!(scaling_task_node->m_scalingTaskStatus & SCALING_TASK_INPUT_BUFFER_NO_MORE_USED))
        {
            // Release input buffer
            TRC( TRC_ID_SCALER, "SCA IN BUFF DONE CB node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
            scaling_task_node->m_scalingCtxNode->m_config.scaler_input_buffer_done_callback(scaling_task_node->m_inputUserData);
            scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_INPUT_BUFFER_NO_MORE_USED;
            m_Statistics.InputBufReleased++;

            // We need to delete scaling_task_node->m_allocatedScalingTaskDoneMsg as it will be never called by low level layer.
            vibe_os_delete_workerdata(scaling_task_node->m_allocatedScalingTaskDoneMsg);
        }

        // Release output buffer
        scaling_task_node->m_scalingCtxNode->m_config.scaler_output_buffer_done_callback(scaling_task_node->m_outputUserData);
        scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_OUTPUT_BUFFER_NO_MORE_USED;
        scaling_task_node->m_scalingCtxNode->m_nbScalingTaskRunning--;
        m_Statistics.OutputBufReleased++;

        // Check if it is the latest buffer of this aborting ctx to release semaphore taken by the Abort
        if(scaling_task_node->m_scalingCtxNode->m_aborting && (scaling_task_node->m_scalingCtxNode->m_nbScalingTaskRunning == 0))
        {
            // Allow completion of Abort() function
            TRC( TRC_ID_SCALER, "SCA SEMA ABORT node=(%p)", scaling_task_node );
            scaling_task_node->m_scalingCtxNode->m_aborting = false;
            vibe_os_up_semaphore(m_abortingSem);
        }

        // delete this scaling task because it is finished
        TRC( TRC_ID_SCALER, "SCA CB REMOVE node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );
        m_pScalingTaskNodePool->ReleaseElement(scaling_task_node);
    }
    else
    {
        TRC( TRC_ID_ERROR, "Msg Aborted cbmsg=%p tn=%p fid=%u msgt=%u msgb=%u", callback_msg, callback_msg->scaling_task_node, callback_msg->scaling_task_node->m_FrameId, callback_msg->scaling_task_node->m_allocatedScalingTaskDoneMsg->aborted, callback_msg->scaling_task_node->m_allocatedScalingBufferDoneMsg->aborted );
    }

    vibe_os_up_semaphore(m_sysProtect);
}


scaler_error_t CScaler::Dispatch(CScalingCtxNode* const scaling_ctx_node, const stm_scaler_task_descr_t *task_descr)
{
    scaler_error_t     error_code        = ERR_NONE;
    CScalingTaskNode  *scaling_task_node = 0;

    // Need protection again scaling thread activity
    if(vibe_os_down_semaphore(m_sysProtect) !=0 )
    {
        error_code = ERR_INTERRUPTED;
        goto exit;
    }

    // Get a free scaling task node
    scaling_task_node = (CScalingTaskNode *)m_pScalingTaskNodePool->GetElement();
    if(!scaling_task_node)
    {
        TRC( TRC_ID_ERROR, "Failed, no more scaling task node available from the pool" );
        // No more place available for allocating a new scaling task, scaler too busy
        error_code = ERR_BUSY;
        goto exit_and_release_sem;
    }

    // Store datas in scaling task node
    scaling_task_node->m_scalingCtxNode                = scaling_ctx_node;
    scaling_task_node->m_FrameId                       = scaling_ctx_node->m_lastFrameId;
    scaling_task_node->m_scalingTaskStatus             = SCALING_TASK_RESET_VALUE;
    scaling_task_node->m_isScalingTaskContentValid     = false;
    scaling_task_node->m_allocatedScalingTaskDoneMsg   = 0;
    scaling_task_node->m_allocatedScalingBufferDoneMsg = 0;

    if(!scaling_task_node->PrepareAndCheckScaling(task_descr))
    {
        TRC( TRC_ID_ERROR, "Failed, to prepare and check scaling task node" );
        error_code = ERR_INTERNAL;
        goto exit_and_release_scaling_task_node;
    }

    // Enqueue scaling task node into the pending queue
    if(!m_pPendingQueue->QueueScalingNode(scaling_task_node))
    {
        TRC( TRC_ID_ERROR, "Failed, to enqueue pending scaling task node" );
        error_code = ERR_INTERNAL;
        goto exit_and_release_scaling_task_node;
    }

    // Update frame identifier for next scaling task to enqueue
    scaling_ctx_node->m_lastFrameId++;

    m_Statistics.PicQueued++;

    TRC( TRC_ID_SCALER, "SCA ENQUEUE SCALING TASK node=(%p) fid=(%u)", scaling_task_node, scaling_task_node->m_FrameId );

    // Send process next scaling task message (if no scaling on-going)
    if(!m_stopDispatchEvent)
    {
        process_next_scaling_task_msg_t *process_next_scaling_task_msg;

        process_next_scaling_task_msg = (process_next_scaling_task_msg_t *) vibe_os_allocate_message(process_next_scaling_task,
                                                                                                     sizeof(process_next_scaling_task_msg_t) );
        if(!process_next_scaling_task_msg)
        {
            TRC( TRC_ID_ERROR, "Failed, to allocate message for processing next scaling task" );
            error_code = ERR_INTERNAL;
            goto exit_and_release_scaling_task_node;
        }

        process_next_scaling_task_msg->scaler_object = this;

        if( !vibe_os_queue_message(m_pMessageThread, process_next_scaling_task_msg) )
        {
            TRC( TRC_ID_ERROR, "Failed, to send message for processing next scaling task" );
            error_code = ERR_INTERNAL;
            vibe_os_delete_workerdata(process_next_scaling_task_msg);
            goto exit_and_release_scaling_task_node;
        }
        else
        {
            m_stopDispatchEvent = true;
        }
    }

    vibe_os_up_semaphore(m_sysProtect);

    return error_code;

exit_and_release_scaling_task_node:

    m_pScalingTaskNodePool->ReleaseElement(scaling_task_node);

exit_and_release_sem:

    vibe_os_up_semaphore(m_sysProtect);

exit:

    return error_code;

}

void CScaler::ReleasingScalingTaskNode(CScalingTaskNode  *scaling_task_node, bool allow_task_done_cb)
{
    vibe_time64_t   time;

    // Call scaling task done callback if not yet done
    if(!(scaling_task_node->m_scalingTaskStatus & SCALING_TASK_DONE))
    {
        if(allow_task_done_cb)
        {
            time = vibe_os_get_system_time();
            TRC( TRC_ID_API_SCALER, "SCA SCALING TD %d CB node=(%p) fid=(%u)", scaling_task_node->m_scalingTaskStatus, scaling_task_node, scaling_task_node->m_FrameId );
            scaling_task_node->m_scalingCtxNode->m_config.scaler_task_done_callback(scaling_task_node->m_outputUserData,
                                                                                    time,
                                                                                    scaling_task_node->m_scalingTaskStatus);
        }
        scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_DONE;

        if(scaling_task_node->m_scalingTaskStatus)
        {
            m_Statistics.PicTreatedValid++;
        }
        else
        {
            m_Statistics.PicTreatedInvalid++;
        }
    }

    // Release input buffer if not yet done
    if(!(scaling_task_node->m_scalingTaskStatus & SCALING_TASK_INPUT_BUFFER_NO_MORE_USED))
    {
        scaling_task_node->m_scalingCtxNode->m_config.scaler_input_buffer_done_callback(scaling_task_node->m_inputUserData);
        scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_INPUT_BUFFER_NO_MORE_USED;
        m_Statistics.InputBufReleased++;
    }

    // Release output buffer if not yet done
    if(!(scaling_task_node->m_scalingTaskStatus & SCALING_TASK_OUTPUT_BUFFER_NO_MORE_USED))
    {
        scaling_task_node->m_scalingCtxNode->m_config.scaler_output_buffer_done_callback(scaling_task_node->m_outputUserData);
        scaling_task_node->m_scalingTaskStatus |= SCALING_TASK_OUTPUT_BUFFER_NO_MORE_USED;
        m_Statistics.OutputBufReleased++;
    }

    // mark callback has aborted (if they were allocated/pushed to low level driver)
    if(scaling_task_node->m_allocatedScalingTaskDoneMsg)
    {
        scaling_task_node->m_allocatedScalingTaskDoneMsg->aborted = true;
    }
    if(scaling_task_node->m_allocatedScalingBufferDoneMsg)
    {
        scaling_task_node->m_allocatedScalingBufferDoneMsg->aborted = true;
    }

    // delete this scaling task because it is finished
    m_pScalingTaskNodePool->ReleaseElement(scaling_task_node);
}

scaler_error_t CScaler::FlushPendingScalingTaskNodes(CScalingCtxNode* const scaling_ctx_node)
{
    CScalingTaskNode  *curr_scaling_task_node;
    CScalingTaskNode  *next_scaling_task_node;
    scaler_error_t     error_code = ERR_NONE;

    // Parse the queue of pending scaling task nodes from the beginning
    curr_scaling_task_node = (CScalingTaskNode *)m_pPendingQueue->GetNextScalingNode(0);
    while( curr_scaling_task_node )
    {

        // get next scaling task node now because current one could be deleted just after.
        next_scaling_task_node = (CScalingTaskNode *)m_pPendingQueue->GetNextScalingNode(curr_scaling_task_node);

        // Check if this scaling task node belong to this aborting scaling context
        if(curr_scaling_task_node->m_scalingCtxNode == scaling_ctx_node)
        {
            // Remove this scaling task from running queue
            if(!m_pPendingQueue->DequeueScalingNode(curr_scaling_task_node))
            {
                TRC( TRC_ID_ERROR, "Failed to remove this scaling task from pending queue" );
                error_code = ERR_INTERNAL;
            }

            ReleasingScalingTaskNode(curr_scaling_task_node, false);

        }

        curr_scaling_task_node = next_scaling_task_node;
    }

    return error_code;
}

scaler_error_t CScaler::FlushRunningScalingTaskNodes(CScalingCtxNode* const scaling_ctx_node, bool *wait_abort_sem)
{
    CScalingTaskNode    *curr_scaling_task_node;
    scaler_error_t       error_code = ERR_NONE;

    *wait_abort_sem = false;

    // Always send abort to lower level driver
    error_code = PerformAborting(scaling_ctx_node);
    if(error_code != ERR_NONE)
    {
        TRC( TRC_ID_ERROR, "Failed to perform aborting for this context %p", scaling_ctx_node );
        return ERR_INTERNAL;
    }

    // Parse the queue of running scaling task node from the beginning
    curr_scaling_task_node = 0;
    while( (curr_scaling_task_node = (CScalingTaskNode *)m_pRunningQueue->GetNextScalingNode(curr_scaling_task_node)) )
    {
        // Check if this scaling task node belong to this aborting scaling context
        if(curr_scaling_task_node->m_scalingCtxNode == scaling_ctx_node)
        {
            // There is at least one scaling task node already dispatched to lower level driver
            // so wait on a semaphore that should be wakeup when lastest input/output
            // buffer callbacks for this scaling context will arrive.
            *wait_abort_sem = true;

            break;
        }
    }

    return error_code;
}

scaler_error_t CScaler::Abort(CScalingCtxNode* const scaling_ctx_node)
{
    bool              wait_abort_sem;
    scaler_error_t    res_flush_pending;
    scaler_error_t    res_flush_running;
    scaler_error_t    error_code = ERR_NONE;

    // Need protection again scaling thread activity
    if(vibe_os_down_semaphore(m_sysProtect) !=0 )
    {
        error_code = ERR_INTERRUPTED;
        goto exit_interrupted;
    }

    TRC( TRC_ID_SCALER, "SCA ABORT scaling_ctx_node=(%p)", scaling_ctx_node );

    // Indicate that abort is on-going for this context
    scaling_ctx_node->m_aborting = true;

    // Flush scaling task node from pending queue
    res_flush_pending = FlushPendingScalingTaskNodes(scaling_ctx_node);
    if(res_flush_pending != ERR_NONE)
    {
        error_code = res_flush_pending;
    }

    // Flush scaling task node from running queue
    res_flush_running = FlushRunningScalingTaskNodes(scaling_ctx_node, &wait_abort_sem);
    if(res_flush_running != ERR_NONE)
    {
        error_code = res_flush_running;
    }

    vibe_os_up_semaphore(m_sysProtect);

    // wait all callbacks to finish the abort
    if(wait_abort_sem)
    {
        TRC( TRC_ID_SCALER, "SCA WAIT SEMA ABORT scaling_ctx_node=(%p)", scaling_ctx_node );
        if(vibe_os_down_semaphore(m_abortingSem) !=0 )
        {
            error_code = ERR_INTERRUPTED;
            goto exit_interrupted;
        }
    }

    return error_code;


exit_interrupted:

        return error_code;

}

void CScaler::ScalingCompletedCbF(CScalingTaskNode *scaling_task_node)
{
    if( !vibe_os_queue_message(m_pMessageThread, scaling_task_node->m_allocatedScalingTaskDoneMsg) )
    {
        TRC( TRC_ID_ERROR, "Failed to send scaling task done msg!" );
    }
}

void CScaler::BufferDoneCbF(CScalingTaskNode *scaling_task_node)
{
    if( !vibe_os_queue_message(m_pMessageThread, scaling_task_node->m_allocatedScalingBufferDoneMsg) )
    {
        TRC( TRC_ID_ERROR, "Failed to send scaling buffer done msg!" );
    }
}

bool CScaler::RegisterStatistics(void)
{
  char Tag[STM_REGISTRY_MAX_TAG_SIZE];

  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_ScalerPicQueued");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicQueued, sizeof(m_Statistics.PicQueued)) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_scalerId );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_scalerId );
  }

  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_ScalerPicTreatedValid");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicTreatedValid, sizeof(m_Statistics.PicTreatedValid)) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_scalerId );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_scalerId );
  }

  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_ScalerPicTreatedInvalid");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicTreatedInvalid, sizeof(m_Statistics.PicTreatedInvalid)) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_scalerId );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_scalerId );
  }

  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_ScalerInputBufReleased");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.InputBufReleased, sizeof(m_Statistics.InputBufReleased)) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_scalerId );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_scalerId );
  }

  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_ScalerOutputBufReleased");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.OutputBufReleased, sizeof(m_Statistics.OutputBufReleased)) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_scalerId );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_scalerId );
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// C scaler interface
//

extern "C" {

int  stm_scaler_open (const uint32_t id, stm_scaler_h *scaler_handle, const stm_scaler_config_t *scaler_config)
{
    int res;

    res = stm_scaler_device_open_scaler(id, scaler_handle, scaler_config);

    if ( not res )
      {
        TRC( TRC_ID_API_SCALER, "id : %u, scaler : %u", id, ((CScaler *)(*scaler_handle)->scaling_ctx_node.m_scalerObject)->GetID() );
      }

    return res;
}

int  stm_scaler_close (const stm_scaler_h scaler_handle )
{
    int         res;

    // Check scaler handle validity
    if(!stm_scaler_handle_valid(scaler_handle))
    {
        TRC( TRC_ID_ERROR, "invalid handle" );
        return -EINVAL;
    }

    TRC( TRC_ID_API_SCALER, "scaler : %u", ((CScaler *)scaler_handle->scaling_ctx_node.m_scalerObject)->GetID() );
    res = stm_scaler_device_close_scaler(scaler_handle);

    return res;
}

int  stm_scaler_get_capabilities (const stm_scaler_h scaler_handle, uint32_t *capabilities)
{
    int            res = 0;
    CScaler       *scaler_object;

    // Check scaler handle validity
    if(!stm_scaler_handle_valid(scaler_handle))
    {
        TRC( TRC_ID_ERROR, "invalid handle" );
        return -EINVAL;
    }

    scaler_object = (CScaler *)(scaler_handle->scaling_ctx_node.m_scalerObject);

    if(!CHECK_ADDRESS(capabilities))
    {
        TRC( TRC_ID_ERROR, "invalid caps address" );
        return -EFAULT;
    }

    if(vibe_os_down_semaphore(scaler_handle->lock) != 0)
    {
        TRC( TRC_ID_ERROR, "sem interrupted" );
        return -EINTR;
    }

    TRC( TRC_ID_API_SCALER, "scaler : %u", scaler_object->GetID() );
    *capabilities = scaler_object->GetCapabilities();

    vibe_os_up_semaphore(scaler_handle->lock);

    return res;
}

int  stm_scaler_dispatch (const stm_scaler_h scaler_handle, const stm_scaler_task_descr_t *task_descr)
{
    int            res = 0;
    CScaler       *scaler_object;
    scaler_error_t error_code;

    // Check scaler handle validity
    if(!stm_scaler_handle_valid(scaler_handle))
    {
        TRC( TRC_ID_ERROR, "invalid handle" );
        return -EINVAL;
    }

    scaler_object = (CScaler *)(scaler_handle->scaling_ctx_node.m_scalerObject);

    if(!CHECK_ADDRESS(task_descr))
    {
        TRC( TRC_ID_ERROR, "invalid task_descr address" );
        return -EFAULT;
    }

    if(vibe_os_down_semaphore(scaler_handle->lock) != 0)
    {
        TRC( TRC_ID_ERROR, "sem interrupted" );
        return -EINTR;
    }

    TRC( TRC_ID_API_SCALER, "scaler : %u", scaler_object->GetID() );
    error_code = scaler_object->Dispatch(&(scaler_handle->scaling_ctx_node), task_descr);
    switch(error_code)
    {
        case ERR_NONE:
            res = 0;
            break;
        case ERR_BUSY:
            TRC( TRC_ID_ERROR, "busy" );
            res = -EBUSY;
            break;
        case ERR_INTERRUPTED:
            TRC( TRC_ID_ERROR, "interrupted" );
            res = -EINTR;
            break;
        default:
            TRC( TRC_ID_ERROR, "fault" );
            res = -EFAULT;
    }

    vibe_os_up_semaphore(scaler_handle->lock);

    return res;
}

int  stm_scaler_abort (const stm_scaler_h scaler_handle )
{
    int            res = 0;
    CScaler       *scaler_object;
    scaler_error_t error_code;

    // Check scaler handle validity
    if(!stm_scaler_handle_valid(scaler_handle))
    {
        TRC( TRC_ID_ERROR, "invalid handle" );
        return -EINVAL;
    }

    scaler_object = (CScaler *)(scaler_handle->scaling_ctx_node.m_scalerObject);

    if(vibe_os_down_semaphore(scaler_handle->lock) != 0)
    {
        TRC( TRC_ID_ERROR, "sem interrupted" );
        return -EINTR;
    }

    TRC( TRC_ID_API_SCALER, "scaler : %u", scaler_object->GetID() );
    error_code = scaler_object->Abort(&(scaler_handle->scaling_ctx_node));
    switch(error_code)
    {
        case ERR_NONE:
            res = 0;
            break;
        case ERR_INTERRUPTED:
            TRC( TRC_ID_ERROR, "interrupted" );
            res = -EINTR;
            break;
        default:
            TRC( TRC_ID_ERROR, "fault" );
            res = -EFAULT;
    }

    vibe_os_up_semaphore(scaler_handle->lock);

    return res;
}


} // extern "C"
