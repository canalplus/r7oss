/***********************************************************************
 *
 * File: scaler/ip/fvdp/fvdp_scaler.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_SCALER_H
#define FVDP_SCALER_H

#include "scaler.h"
#include "fvdp.h"

#ifdef __cplusplus

class CScalerDevice;


typedef enum
{
    FVDP_CHANNEL_MAIN,
    FVDP_CHANNEL_PIP,
    FVDP_CHANNEL_AUX,
    FVDP_CHANNEL_ENC
} FVDP_ChannelType_t;


typedef struct {
    class CFvdpScaler *fvdp_class;
    FVDP_CH_Handle_t   fvdp_ch_handle;
    DMA_Area           fvdp_ch_memory;
    FVDP_PSIState_t    fvdp_prev_tnr_state;
} hardware_ctx_t;

class CFvdpScaler: public CScaler
{
public:
    CFvdpScaler(uint32_t id, const CScalerDevice *pDev, FVDP_ChannelType_t scalerType);
    virtual ~CFvdpScaler();

    virtual bool              Create(void);

protected:
    virtual scaler_error_t    PerformOpening(CScalingCtxNode* const scaling_ctx_node);
    virtual scaler_error_t    PerformClosing(CScalingCtxNode* const scaling_ctx_node);
    virtual scaler_error_t    PerformScaling(CScalingCtxNode* const scaling_ctx_node, CScalingTaskNode *scaling_task_node);
    virtual scaler_error_t    PerformAborting(CScalingCtxNode* const scaling_ctx_node);


    FVDP_ChannelType_t  m_scalerType;
    FVDP_CH_t           m_fvdpChannel;

};





#endif /* __cplusplus */

#endif /* FVDP_SCALER_H */
