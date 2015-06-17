/***********************************************************************
 *
 * File: scaler/ip/blitter/blitter_task_node.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef BLITTER_TASK_NODE_H
#define BLITTER_TASK_NODE_H

#include <stm_scaler.h>
#include "scaling_task_node.h"

extern "C" {
#include "blitter.h"
};

#ifdef __cplusplus

typedef struct {
    stm_blitter_surface_format_t     format;
    stm_blitter_surface_colorspace_t cspace;
    stm_blitter_dimension_t          buffer_dimension;
    stm_blitter_surface_address_t    buffer_add;
    unsigned long                    buffer_size;
    unsigned long                    pitch;
    stm_blitter_rect_t               crop_rect;
} blitter_params_t;

class CBlitterTaskNode : public CScalingTaskNode
{
public:
    CBlitterTaskNode() : CScalingTaskNode()
      {
        vibe_os_zero_memory( &m_blitterSrce, sizeof( m_blitterSrce ));
        vibe_os_zero_memory( &m_blitterDest, sizeof( m_blitterDest ));
        m_blitterFlags = 0;
      }

    virtual bool        PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr);

    blitter_params_t    m_blitterSrce;    // blitter source parameters
    blitter_params_t    m_blitterDest;    // blitter destination parameters
    uint32_t            m_blitterFlags;   // Or'd value of stm_blitter_surface_blitflags_t

protected:
    bool                PrepareAndCheckScalerFlags(const stm_scaler_task_descr_t *task_descr);
    bool                PrepareSourceSurface(const stm_scaler_task_descr_t *task_descr,
                                             area_config_t                 *area_config);
    bool                PrepareDestinationSurface(const stm_scaler_task_descr_t *task_descr,
                                                  area_config_t                 *area_config);
}; /* CBlitterTaskNode */


#endif /* __cplusplus */

#endif /* BLITTER_TASK_NODE_H */

