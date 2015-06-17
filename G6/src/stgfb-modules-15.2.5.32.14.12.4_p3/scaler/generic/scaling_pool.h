/***********************************************************************
 *
 * File: scaler/generic/scaling_pool.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALING_POOL_H
#define SCALING_POOL_H

#ifdef __cplusplus

#include "scaling_element.h"

// Pool of CScalingElement for static allocation.
class CScalingPool
{
public:
                             CScalingPool(void) { m_maxElements = 0; }
    virtual                 ~CScalingPool(void) {}

    // Allocate the pool of scaling elements
    virtual bool             Create(uint32_t maxElements);

    // Get a scaling element from the pool of CScalingElement
    virtual CScalingElement *GetElement(void);
    // Release a used node to the pool
    virtual void             ReleaseElement(CScalingElement *Element);

protected:
    void                     MarkAllFree(void);
    void                     CheckAllFree(void);
    virtual CScalingElement *GetElement(uint32_t index) = 0;

    uint32_t          m_maxElements;    // Maximum number of scaling element in the Pool of scaling elements

}; /* CScalingPool */

#endif /* __cplusplus */

#endif /* SCALING_POOL_H */
