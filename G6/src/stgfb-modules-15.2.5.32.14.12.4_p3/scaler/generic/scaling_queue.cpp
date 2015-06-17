/***********************************************************************
 *
 * File: scaler/generic/scaling_queue.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>

#include "scaling_queue.h"


CScalingQueue::CScalingQueue(element_type_t elementType)
{
    m_elementType = elementType;
    m_pNodeHead   = 0;
}

CScalingQueue::~CScalingQueue()
{
#ifdef DEBUG
    // To check if scaler queue is empty.
    if(m_pNodeHead)
    {
        TRC( TRC_ID_ERROR, "Failed, scaler queue is not empty" );
    }
#endif
}

// Function returning the node following the one provided in argument
// In case of null argument, it will provide the head of the queue
CScalingNode *CScalingQueue::GetNextScalingNode(CScalingNode *node)
{
    CScalingNode   *pCurrentNode = node;
    CScalingNode   *pNextNode;

    // Check node validity
    // WARNING: The test condition is not the same as in other functions of this class because, here,
    // pCurrentNode can be null when we want to search from the beginning of the queue.
    // So, only the case where (pCurrentNode!=0) AND (pCurrentNode->isUsed==false OR pCurrentNode->m_elementType!=m_elementType) is an error.
    if ( (pCurrentNode) && ( (!pCurrentNode->m_isUsed) || (pCurrentNode->m_elementType != m_elementType) ) )
    {
        TRC( TRC_ID_ERROR, "Tried to find the next node of an invalid node!" );
        // Search for the head of the queue instead
        pCurrentNode = 0;
    }

    // In case of null node, return the head of the queue if valid
    if (!pCurrentNode)
    {
        pNextNode = m_pNodeHead;

        // If the node exist but is invalid, return null
        if(pNextNode && !pNextNode->m_isUsed)
            pNextNode = 0;
    }
    else
    {
        pNextNode = pCurrentNode->m_pNextNode;
    }

    return (pNextNode);
}

bool CScalingQueue::QueueScalingNode(CScalingNode *node)
{
    bool           res   = true;
    CScalingNode  *pNode = node;
    CScalingNode  *pTail;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    // Check node validity
    if ( (!pNode) || (!pNode->m_isUsed) || (pNode->m_elementType != m_elementType) )
    {
        TRC( TRC_ID_ERROR, "Tried to queue an invalid node!" );
        res = false;
        goto exit_queue_node;
    }

    // Queue it a the tail of the queue
    if (m_pNodeHead)
    {
        pTail = GetTail(m_pNodeHead);  // This can not fail
        pTail->m_pNextNode = pNode;
    }
    else
    {
        // This is the first node in the queue
        m_pNodeHead = pNode;
    }

    // Indicate that it is the last node in the queue
    pNode->m_pNextNode = 0;

    TRC( TRC_ID_UNCLASSIFIED, "Queued new buffer in node %p", pNode );

exit_queue_node:

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );

    return res;
}

// Release a node
// The queue is updated to remove the reference to this node.
bool CScalingQueue::DequeueScalingNode(CScalingNode *node)
{
    bool            res   = true;
    CScalingNode   *pNodeToRelease = node;
    CScalingNode   *pNode;
    bool            nodeFound = false;

    // Check node validity
    if ( (!pNodeToRelease) || (!pNodeToRelease->m_isUsed) || (pNodeToRelease->m_elementType != m_elementType) )
    {
        TRC( TRC_ID_ERROR, "Tried to dequeue an invalid node!" );
        res = false;
        goto exit_release_node;
    }

    // Remove any reference to this node in the Queue
    // NB: The node can be present only one time in the Queue otherwise the linked list would loop forever
    if (pNodeToRelease == m_pNodeHead)
    {
        m_pNodeHead = pNodeToRelease->m_pNextNode;
        pNodeToRelease->m_pNextNode = 0;
        nodeFound = true;
    }
    else
    {
        pNode = m_pNodeHead;

        while ( (pNode != 0) && (nodeFound == false) )
        {
            if (pNode->m_pNextNode == pNodeToRelease)
            {
                pNode->m_pNextNode = pNodeToRelease->m_pNextNode;
                nodeFound = true;
            }
            pNode = pNode->m_pNextNode;
        }
    }

exit_release_node:

    return (res);
}

CScalingNode *CScalingQueue::GetTail(CScalingNode *node)
{
    CScalingNode  *pNode = node;

    while (pNode->m_pNextNode)
    {
            pNode = pNode->m_pNextNode;
    }

    return (pNode);
}

