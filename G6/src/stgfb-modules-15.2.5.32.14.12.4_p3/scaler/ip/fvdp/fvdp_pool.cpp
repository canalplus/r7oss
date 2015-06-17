/***********************************************************************
 *
 * File: scaler/ip/fvdp/fvdp_pool.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include "fvdp_pool.h"
#include "fvdp_task_node.h"


bool CFvdpPool::Create(uint32_t maxElements)
{
    // Indicate the number of scaling task elements in the pool
    CScalingPool::Create(maxElements);

    // Allocate the scaling task elements
    m_fvdpTaskNodePool = new CFvdpTaskNode[m_maxElements];
    if(!m_fvdpTaskNodePool)
    {
        TRC( TRC_ID_ERROR, "failed to allocate pool of scaling task node" );
        return false;
    }

    // Set all scaling task elements to free state
    MarkAllFree();

    return true;
}

inline CScalingElement *CFvdpPool::GetElement(uint32_t index)
{
    return(&(m_fvdpTaskNodePool[index]));
}

CFvdpPool::~CFvdpPool(void)
{
    // Check that all scaling task elements were in free state
    CheckAllFree();

    delete [] m_fvdpTaskNodePool;
}


