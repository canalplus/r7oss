/***********************************************************************
 *
 * File: scaler/ip/blitter/blitter_scaler.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef BLITTER_SCALER_H
#define BLITTER_SCALER_H

#include "scaler.h"
#include "blitter_task_node.h"

extern "C" {
#include "blitter.h"
};



#ifdef __cplusplus

class CScalerDevice;

class CBlitterScaler: public CScaler
{
public:
    CBlitterScaler(uint32_t scaler_id, uint32_t blitter_id, const CScalerDevice *pDev);
    virtual ~CBlitterScaler();

    virtual bool             Create(void);

protected:
    virtual scaler_error_t   PerformOpening(CScalingCtxNode* const scaling_ctx_node);
    virtual scaler_error_t   PerformClosing(CScalingCtxNode* const scaling_ctx_node);
    virtual scaler_error_t   PerformScaling(CScalingCtxNode* const scaling_ctx_node, CScalingTaskNode *scaling_task_node);
    virtual scaler_error_t   PerformAborting(CScalingCtxNode* const scaling_ctx_node);

    bool ApplyBlit(const CBlitterTaskNode *blitter_task_node);
    void AdjustBorderRectToHWConstraints(stm_blitter_dimension_t *buffer_dim, stm_blitter_rect_t *black_rect);
    bool FillPictureBorderOfDestSurface(const CBlitterTaskNode *blitter_task_node);
    bool PrepareDestSurfaceToBlit(const CBlitterTaskNode *blitter_task_node);
    bool PrepareSrcSurfaceToBlit(const CBlitterTaskNode *blitter_task_node);

    int32_t                m_blitterId;      // blitter identifier
    stm_blitter_t         *m_blitterHandle;  // blitter handle
    stm_blitter_surface_t *m_SrcSurface;     // blitter source surface
    stm_blitter_surface_t *m_DestSurface;    // blitter destination surface

};

#endif /* __cplusplus */

#endif /* BLITTER_SCALER_H */

