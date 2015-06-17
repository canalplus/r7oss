/***********************************************************************
 *
 * File: scaler/ip/blitter/blitter_task_node.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include "blitter_pool.h"
#include "blitter_task_node.h"



bool CBlitterPool::Create(uint32_t maxElements)
{
    // Indicate the number of scaling task elements in the pool
    CScalingPool::Create(maxElements);

    // Allocate the scaling task elements
    m_blitterTaskNodePool = new CBlitterTaskNode[m_maxElements];
    if(!m_blitterTaskNodePool)
    {
        TRC( TRC_ID_ERROR, "failed to allocate pool of scaling task node" );
        return false;
    }

    // Set all scaling task elements to free state
    MarkAllFree();

    return true;
}

inline CScalingElement *CBlitterPool::GetElement(uint32_t index)
{
    return(&(m_blitterTaskNodePool[index]));
}

CBlitterPool::~CBlitterPool(void)
{
    // Check that all scaling task elements were in free state
    CheckAllFree();

    delete [] m_blitterTaskNodePool;
}


