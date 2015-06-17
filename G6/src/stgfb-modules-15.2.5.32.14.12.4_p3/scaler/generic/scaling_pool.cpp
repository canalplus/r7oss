/***********************************************************************
 *
 * File: scaler/generic/scaling_pool.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>

#include "scaling_pool.h"
#include "scaling_ctx_node.h"
#include "scaling_task_node.h"


bool CScalingPool::Create(uint32_t maxElements)
{
    m_maxElements = maxElements;
    return true;
}

CScalingElement * CScalingPool::GetElement(void)
{
    CScalingElement *scaling_element = 0;
    CScalingElement *current_element;

    for (uint32_t i = 0; i < m_maxElements; i++)
    {
        current_element = GetElement(i);
        if (!current_element->m_isUsed)
        {
            current_element->m_isUsed = true;
            scaling_element = current_element;
            break;
        }
    }

    return scaling_element;
}

void CScalingPool::ReleaseElement(CScalingElement *Element)
{
    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if(Element && Element->m_isUsed)
    {
        Element->m_isUsed = false;
        // Do not erase type of scaling element
    }
    else
    {
        TRC( TRC_ID_ERROR, "Failed, scaling element %p is already erased", Element );
    }

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}

void CScalingPool::MarkAllFree(void)
{
    for (uint32_t i = 0; i < m_maxElements; i++)
    {
        GetElement(i)->m_isUsed = false;
    }
}

void CScalingPool::CheckAllFree(void)
{
    for (uint32_t i = 0; i < m_maxElements; i++)
    {
        if (GetElement(i)->m_isUsed)
        {
            TRC( TRC_ID_ERROR, "Failed, scaling element still present in the pool during its destruction [%u]", i );
        }
    }
}

