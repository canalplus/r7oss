/***********************************************************************
 *
 * File: scaler/ip/fvdp/fvdp_pool.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_POOL_H
#define FVDP_POOL_H

#ifdef __cplusplus

#include "scaling_pool.h"
#include "fvdp_task_node.h"

class CFvdpPool : public CScalingPool
{
public:
                            CFvdpPool(void) : CScalingPool() { m_fvdpTaskNodePool = 0; }
    virtual                ~CFvdpPool(void);

    // Allocate the pool of scaling elements
    virtual bool            Create(uint32_t maxElements);

    inline CScalingElement *GetElement(uint32_t index);

protected:
    CFvdpTaskNode          *m_fvdpTaskNodePool; // Pool of fvdp scaling tasks

}; /* CFvdpPool */

#endif /* __cplusplus */

#endif /* FVDP_POOL_H */

