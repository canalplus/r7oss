/***********************************************************************
 *
 * File: scaler/ip/blitter/blitter_pool.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef BLITTER_POOL_H
#define BLITTER_POOL_H


#ifdef __cplusplus

#include "scaling_pool.h"
#include "blitter_task_node.h"


class CBlitterPool : public CScalingPool
{
public:
                            CBlitterPool(void) : CScalingPool() { m_blitterTaskNodePool = 0; }
    virtual                ~CBlitterPool(void);

    // Allocate the pool of scaling elements
    virtual bool            Create(uint32_t maxElements);

    inline CScalingElement *GetElement(uint32_t index);

protected:
    CBlitterTaskNode       *m_blitterTaskNodePool; // Pool of blitter scaling tasks

}; /* CBlitterPool */


#endif /* __cplusplus */

#endif /* BLITTER_POOL_H */

