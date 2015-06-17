/***********************************************************************
 *
 * File: scaler/generic/scaler.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALER_H
#define SCALER_H

#ifdef __cplusplus

#include "scaler_device.h"
#include "vibe_os.h"

#include "scaling_ctx_node.h"
#include "scaling_task_node.h"
#include "scaling_pool.h"
#include "scaling_queue.h"

class CScalerDevice;
class CScaler;
class CScalingTaskNode;

#define MAX_SCALING_TASKS 128

typedef enum
{
    ERR_NONE,
    ERR_NO_MEMORY,
    ERR_INTERNAL,
    ERR_BUSY,
    ERR_INTERRUPTED,
} scaler_error_t;

typedef enum
{
    SCALING_TASK_RESET_VALUE                = 0,       // reset value
    SCALING_TASK_DONE                       = (1<<0),  // indicate if this scaling task is done
    SCALING_TASK_INPUT_BUFFER_NO_MORE_USED  = (1<<1),  // indicate input buffer of this scaling task is still used
    SCALING_TASK_OUTPUT_BUFFER_NO_MORE_USED = (1<<2),  // indicate output buffer of this scaling task is still used
} scaling_task_status_t;

typedef struct
{
    CScaler* scaler_object;                 // pointer on scaler object
} process_next_scaling_task_msg_t;

// Scaler statistics
typedef struct
{
    uint32_t                 PicQueued;         // Number of picture enqueued (requested to be dispatched) for this scaler instance
    uint32_t                 PicTreatedValid;   // Number of valid picture rescaled
    uint32_t                 PicTreatedInvalid; // Number of invalid picture rescaled
    uint32_t                 InputBufReleased;  // Number of input buffer released (buffer was taken when picture was enqueued)
    uint32_t                 OutputBufReleased; // Number of output buffer released (buffer was taken when picture was enqueued)
} scaling_stat_t;


class CScaler
{
public:
    CScaler(uint32_t id, uint32_t caps, const CScalerDevice *pDev);
    virtual ~CScaler();

    uint32_t                  GetID(void)            const { return m_scalerId; }
    uint32_t                  GetCapabilities(void)  const { return m_scalerCaps; }
    const CScalerDevice      *GetScalerDevice(void)  const { return m_pScalerDevice; }

    virtual bool              Create   (void);
    virtual scaler_error_t    Open     (CScalingCtxNode* const scaling_ctx_node, const stm_scaler_config_t *scaler_config);
    virtual scaler_error_t    Close    (CScalingCtxNode* const scaling_ctx_node);
    virtual scaler_error_t    Dispatch (CScalingCtxNode* const scaling_ctx_node, const stm_scaler_task_descr_t *task_descr);
    virtual scaler_error_t    Abort    (CScalingCtxNode* const scaling_ctx_node);

    void                      ScalingCompletedCbF(CScalingTaskNode *scaling_task);
    void                      BufferDoneCbF(CScalingTaskNode *scaling_task);

    void                      ProcessNextScalingTaskNode(void);
    void                      ScalingTaskDoneCbTreatment(void *data);
    void                      ScalingBufferDoneCbTreatment(void *data);

    bool                      RegisterStatistics(void);

protected:
    virtual scaler_error_t    PerformOpening(CScalingCtxNode* const scaling_ctx_node) = 0;
    virtual scaler_error_t    PerformClosing(CScalingCtxNode* const scaling_ctx_node) = 0;
    virtual scaler_error_t    PerformScaling(CScalingCtxNode* const scaling_ctx_node, CScalingTaskNode *scaling_task_node) = 0;
    virtual scaler_error_t    PerformAborting(CScalingCtxNode* const scaling_ctx_node) = 0;

    scaler_error_t            ProcessScalingTaskNode(CScalingTaskNode *scaling_task_node);
    void                      ReleasingScalingTaskNode(CScalingTaskNode *scaling_task_node, bool allow_task_done_cb);

    scaler_error_t            FlushPendingScalingTaskNodes(CScalingCtxNode* const scaling_ctx_node);
    scaler_error_t            FlushRunningScalingTaskNodes(CScalingCtxNode* const scaling_ctx_node, bool *wait_abort_sem);

    uint32_t              m_scalerId;        // Scaler identifier
    uint32_t              m_scalerCaps;      // Scaler capabilities (Or'd of stm_scaler_capabilities_t values)
    const CScalerDevice  *m_pScalerDevice;   // Pointer to scaler device object

    VIBE_OS_MessageThread_t  m_pMessageThread;    // Message thread

    bool                  m_stopDispatchEvent;    // Stop dispatch event until all pending/running scaling tasks are done.
    void                * m_abortingSem;          // Aborting semaphore: wait completion of abort inside Abort() function

    void                * m_sysProtect;           // system protection: protect scaler driver behavior

    CScalingQueue        *m_pCtxQueue;               // Queue of opened scaling context (scaling context are allocated dynamically so no Pool)
    CScalingPool         *m_pScalingTaskNodePool;    // Pool of scaling task nodes
    CScalingQueue        *m_pPendingQueue;           // Queue of scaling tasks requested by user but not yet started on the hardware
    CScalingQueue        *m_pRunningQueue;           // Queue of scaling tasks requested by user and on-going on the hardware

    scaling_stat_t        m_Statistics;              // Scaler statistics

}; /* CScaler */

#endif /* __cplusplus */

#endif /* SCALER_H */
