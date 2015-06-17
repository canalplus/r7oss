/***********************************************************************
 *
 * File: scaler/generic/scaling_ctx_node.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALING_CTX_NODE_H
#define SCALING_CTX_NODE_H

#ifdef __cplusplus

#include <stm_scaler.h>
#include "scaling_node.h"

class CScaler;

class CScalingCtxNode : public CScalingNode
{
public:
    CScalingCtxNode() : CScalingNode(TYPE_SCALING_CTX)
      {
        m_scalerObject = 0;
        vibe_os_zero_memory( &m_config, sizeof( m_config ));
        m_aborting = false;
        m_lastFrameId = 0;
        m_nbScalingTaskRunning = 0;
        m_HwCtx = 0;
      }

    CScaler             *m_scalerObject; // Scaler object
    stm_scaler_config_t  m_config;        // Settings and callback functions specific to this scaler instance
    bool                 m_aborting;      // To know if an abort is on-going for this context
    uint32_t             m_lastFrameId; // Last frame identifier for this context
    uint32_t             m_nbScalingTaskRunning; // Counter of scaling task node currently handle by low level driver (fvdp or blitter)
    void*                m_HwCtx;        // Pointer on private datas of blitter or fvdp scaler (mainly associated to hardware config)
}; /* CScalingCtxNode */

#endif /* __cplusplus */

#endif /* SCALING_CTX_NODE_H */

