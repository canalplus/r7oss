/***********************************************************************
 *
 * File: scaler/ip/fvdp/fvdp_scaler.cpp
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
#include "fvdp_pool.h"


#define MAX_HD_DISPLAY_HSIZE     1920
#define MAX_HD_DISPLAY_VSIZE     1088


static void scaling_completed_cb_f(void* UserData, bool Status);
static void buffer_done_cb_f(void* UserData);


CFvdpScaler::CFvdpScaler(uint32_t                   id,
                         const CScalerDevice       *pDev,
                         FVDP_ChannelType_t         scalerType): CScaler(id, STM_SCALER_CAPS_TNR, pDev)
{
    m_scalerType = scalerType;
    m_fvdpChannel = FVDP_ENC;
}


CFvdpScaler::~CFvdpScaler()
{
    delete(m_pScalingTaskNodePool);
}

bool CFvdpScaler::Create(void)
{
    if(!CScaler::Create())
    {
        TRC( TRC_ID_ERROR, "Base class Create failed" );
        return false;
    }

    // Create the pool of scaling task nodes
    m_pScalingTaskNodePool = (CScalingPool *)new CFvdpPool();
    if( (!m_pScalingTaskNodePool) || (!m_pScalingTaskNodePool->Create(MAX_SCALING_TASKS)) )
    {
        TRC( TRC_ID_ERROR, "Failed to allocate pool of scaling task nodes" );
        return false;
    }

    return true;
}

scaler_error_t CFvdpScaler::PerformOpening(CScalingCtxNode* const scaling_ctx_node)
{
    // Save in the scaler_handle->specific part, the FvdpHandle

    FVDP_Result_t     fvdp_err;
    uint32_t          MemSize;
    FVDP_Context_t    FVDPContext;
    scaler_error_t    error_code = ERR_NONE;
    hardware_ctx_t   *hw_ctx = 0;

    switch (m_scalerType)
    {
        /*
        case FVDP_CHANNEL_MAIN:
        case FVDP_CHANNEL_PIP:
        case FVDP_CHANNEL_AUX:
        */

        case FVDP_CHANNEL_ENC:
            m_fvdpChannel = FVDP_ENC;
            FVDPContext.ProcessingType = FVDP_SPATIAL;
            FVDPContext.MaxWindowSize.Width = MAX_HD_DISPLAY_HSIZE;
            FVDPContext.MaxWindowSize.Height = MAX_HD_DISPLAY_VSIZE;
            break;

        default:
            TRC( TRC_ID_ERROR, "Invalid m_scalerType (%d)!", m_scalerType );
            error_code = ERR_INTERNAL;
            goto exit;
    }

    // Get required memory size to allocate according to FVDP processing type and max output window
    fvdp_err = FVDP_GetRequiredVideoMemorySize(m_fvdpChannel, FVDPContext, &MemSize);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to get require video memory size. FVDP channel=%d (%d)!", m_fvdpChannel, fvdp_err );
        error_code = ERR_INTERNAL;
        goto exit;
    }

    // Allocate hardware context
    hw_ctx = new hardware_ctx_t;
    if(!hw_ctx)
    {
        TRC( TRC_ID_ERROR, "Failed to allocate hardware context. FVDP channel=%d !", m_fvdpChannel );
        error_code = ERR_NO_MEMORY;
        goto exit;
    }
    //vibe_os_zero_memory(hw_ctx, sizeof(hardware_ctx_t));
    hw_ctx->fvdp_class = this;

    if(MemSize)
    {
        // Allocate channel's memory pool for FVDP driver use. 64 byte alignment.
        vibe_os_allocate_dma_area(&(hw_ctx->fvdp_ch_memory), MemSize, 64, SDAAF_VIDEO_MEMORY);
        if(hw_ctx->fvdp_ch_memory.pMemory == 0)
        {
            TRC( TRC_ID_ERROR, "Failed to allocate Channel's working memory" );
            error_code = ERR_NO_MEMORY;
            goto exit_delete_hw_ctx;
        }
        TRC( TRC_ID_MAIN_INFO, "CFvdpScaler: Successed to allocate DMA area:\n size=0x%x\n mem=%p", hw_ctx->fvdp_ch_memory.ulDataSize, hw_ctx->fvdp_ch_memory.pMemory );
    }
    else
    {
        // No DMA area was allocated
        vibe_os_zero_memory(&(hw_ctx->fvdp_ch_memory), sizeof(hw_ctx->fvdp_ch_memory));
        TRC( TRC_ID_MAIN_INFO, "CFvdpScaler: no DMA area to allocate" );
    }

    fvdp_err = FVDP_OpenChannel(&(hw_ctx->fvdp_ch_handle), m_fvdpChannel, (uint32_t)hw_ctx->fvdp_ch_memory.ulPhysical, MemSize, FVDPContext);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to open FVDP channel %d (%d)!", m_fvdpChannel, fvdp_err );
        error_code = ERR_INTERNAL;
        goto exit_free_dma;
    }
    TRC( TRC_ID_MAIN_INFO, "FVDP Channel %d opened", m_fvdpChannel );

    fvdp_err = FVDP_SetBufferCallback(hw_ctx->fvdp_ch_handle,
            &(scaling_completed_cb_f),
            &(buffer_done_cb_f) );
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to set buffer callback for FVDP channel %d (%d)!", m_fvdpChannel, fvdp_err );
        error_code = ERR_INTERNAL;
        goto exit_close_fvdp;
    }

    // Force default configuration
    hw_ctx->fvdp_prev_tnr_state = FVDP_PSI_OFF;

    fvdp_err = FVDP_SetPSIState(hw_ctx->fvdp_ch_handle, FVDP_TNR, hw_ctx->fvdp_prev_tnr_state);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to disable TNR for FVDP channel %d (%d)!", m_fvdpChannel, fvdp_err );
        goto exit_close_fvdp;
    }

    scaling_ctx_node->m_HwCtx = (void*) hw_ctx;

    return ERR_NONE;

exit_close_fvdp:
    fvdp_err = FVDP_CloseChannel((hw_ctx->fvdp_ch_handle));
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to set buffer callback for FVDP channel %d (%d)!", m_fvdpChannel, fvdp_err );
    }

exit_free_dma:
    if(hw_ctx->fvdp_ch_memory.pMemory)
    {
        vibe_os_free_dma_area(&(hw_ctx->fvdp_ch_memory));
        hw_ctx->fvdp_ch_memory.pMemory = 0;
    }

exit_delete_hw_ctx:
    delete(hw_ctx);
    scaling_ctx_node->m_HwCtx = 0;

exit:
    return error_code;
}

scaler_error_t CFvdpScaler::PerformClosing(CScalingCtxNode* const scaling_ctx_node)
{
    FVDP_Result_t     fvdp_err;
    hardware_ctx_t   *hw_ctx = (hardware_ctx_t *)scaling_ctx_node->m_HwCtx;
    scaler_error_t    error_code = ERR_NONE;

    TRC( TRC_ID_MAIN_INFO, "FVDP Channel %d closed", m_fvdpChannel );

    // Close FVDP channel
    fvdp_err = FVDP_CloseChannel(hw_ctx->fvdp_ch_handle);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to close FVDP channel %d (%d)!", m_fvdpChannel, fvdp_err );
        error_code = ERR_INTERNAL;
    }

    // Free DMA Area allocated in PerformOpening
    if(hw_ctx->fvdp_ch_memory.pMemory)
    {
        vibe_os_free_dma_area(&(hw_ctx->fvdp_ch_memory));
        hw_ctx->fvdp_ch_memory.pMemory = 0;
    }

    // Free hardware context allocated in PerformOpening
    delete(hw_ctx);
    scaling_ctx_node->m_HwCtx = 0;

    return error_code;
}


scaler_error_t CFvdpScaler::PerformScaling(CScalingCtxNode* const scaling_ctx_node, CScalingTaskNode *scaling_task_node)
{
    FVDP_Result_t     fvdp_err;
    hardware_ctx_t   *hw_ctx = (hardware_ctx_t *)scaling_ctx_node->m_HwCtx;
    CFvdpTaskNode    *fvdp_task_node = (CFvdpTaskNode *)scaling_task_node;

    // Update TNR flag if it change of state
    if(hw_ctx->fvdp_prev_tnr_state != fvdp_task_node->m_TnrState)
    {
        fvdp_err = FVDP_SetPSIState(hw_ctx->fvdp_ch_handle, FVDP_TNR, fvdp_task_node->m_TnrState);
        if (fvdp_err != FVDP_OK)
        {
            TRC( TRC_ID_ERROR, "Failed to %s TNR for FVDP channel %d (%d)!", ((fvdp_task_node->m_TnrState==FVDP_PSI_OFF)?"disable":"enable"), hw_ctx->fvdp_ch_handle, fvdp_err );
            return ERR_INTERNAL;
        }
        hw_ctx->fvdp_prev_tnr_state = fvdp_task_node->m_TnrState;
    }

    // Set Input Video Info
    fvdp_err = FVDP_SetInputVideoInfo(hw_ctx->fvdp_ch_handle, fvdp_task_node->m_InputVideoInfo);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to set input video info for FVDP channel %d (%d)!", hw_ctx->fvdp_ch_handle, fvdp_err );
        return ERR_INTERNAL;
    }


    // Set Input Cropping Window
    fvdp_err = FVDP_SetCropWindow(hw_ctx->fvdp_ch_handle, fvdp_task_node->m_InputCropWindow);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to set crop window for FVDP channel %d (%d)!", hw_ctx->fvdp_ch_handle, fvdp_err );
        return ERR_INTERNAL;
    }


    // Set Output Video Info
    fvdp_err = FVDP_SetOutputVideoInfo(hw_ctx->fvdp_ch_handle, fvdp_task_node->m_OutputVideoInfo);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to set output video info for FVDP channel %d (%d)!", hw_ctx->fvdp_ch_handle, fvdp_err );
        return ERR_INTERNAL;
    }


    // Set Output Active Window
    fvdp_err = FVDP_SetOutputWindow(hw_ctx->fvdp_ch_handle, &(fvdp_task_node->m_OutputActiveWindow));
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to set output active window for FVDP channel %d (%d)!", hw_ctx->fvdp_ch_handle, fvdp_err );
        return ERR_INTERNAL;
    }

    // Queue Buffer to FVDP
    fvdp_err = FVDP_QueueBuffer(hw_ctx->fvdp_ch_handle, (void *)fvdp_task_node,
            fvdp_task_node->m_InputLumaAddr, fvdp_task_node->m_InputChromaOffset,
            fvdp_task_node->m_OutputLumaAddr, fvdp_task_node->m_OutputChromaOffset);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to queue buffer for FVDP channel %d (%d)!", hw_ctx->fvdp_ch_handle, fvdp_err );
        return ERR_INTERNAL;
    }

    return ERR_NONE;
}

scaler_error_t CFvdpScaler::PerformAborting(CScalingCtxNode* const scaling_ctx_node)
{
    FVDP_Result_t     fvdp_err;
    hardware_ctx_t   *hw_ctx = (hardware_ctx_t *)scaling_ctx_node->m_HwCtx;
    scaler_error_t    error_code = ERR_NONE;

    fvdp_err = FVDP_QueueFlush(hw_ctx->fvdp_ch_handle, TRUE);
    if (fvdp_err != FVDP_OK)
    {
        TRC( TRC_ID_ERROR, "Failed to flush FVDP channel %d (%d)!", m_fvdpChannel, fvdp_err );
        error_code = ERR_INTERNAL;
    }

    return error_code;
}

void scaling_completed_cb_f(void* UserData, bool Status)
{
    CScalingTaskNode *scaling_task_node      = (CScalingTaskNode *) UserData;

    scaling_task_node->m_isScalingTaskContentValid = Status;

    scaling_task_node->m_scalingCtxNode->m_scalerObject->ScalingCompletedCbF(scaling_task_node);
}

void buffer_done_cb_f(void* UserData)
{
    CScalingTaskNode *scaling_task_node      = (CScalingTaskNode *) UserData;

    scaling_task_node->m_scalingCtxNode->m_scalerObject->BufferDoneCbF(scaling_task_node);
}


