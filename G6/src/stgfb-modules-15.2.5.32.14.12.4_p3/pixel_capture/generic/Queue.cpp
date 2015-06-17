/***********************************************************************
 *
 * File: pixel_capture/generic/Queue.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>

#include "Queue.h"

// Structure of one node
struct capture_node_s
{
    bool             isUsed;
    capture_node_s  *pNextNode;
    void            *pDataNode;
};

CCaptureQueue::CCaptureQueue(uint32_t maxNodes, bool QueueProtectedByLock)
{
    m_QueueProtectedByLock  = QueueProtectedByLock;
    m_MaxNodes              = maxNodes;
    m_pNodeHead             = 0;
    m_lock                  = 0;
    m_pCaptureNodes         = 0;

}

CCaptureQueue::~CCaptureQueue()
{
#ifdef DEBUG
    // To check if CaptureNodes are all unused before deleting them.
    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    for (uint32_t i = 0; i < m_MaxNodes; i++)
    {
        if (m_pCaptureNodes[i].isUsed)
        {
            TRC( TRC_ID_ERROR, "capture node always used during destruction [%u]", i );
        }
    }

    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);
#endif
    vibe_os_delete_resource_lock(m_lock);

    vibe_os_free_memory(m_pCaptureNodes);
}

bool CCaptureQueue::Create(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    if (m_QueueProtectedByLock)
    {
        m_lock = vibe_os_create_resource_lock();
        if(!m_lock)
           return false;
    }

    // Allocate a pool of nodes
    m_pCaptureNodes = (capture_node_h) vibe_os_allocate_memory (m_MaxNodes * sizeof(struct capture_node_s) );
    if(!m_pCaptureNodes)
    {
        TRC( TRC_ID_ERROR, "failed to allocate queue" );
        return false;
    }
    vibe_os_zero_memory(m_pCaptureNodes, m_MaxNodes * sizeof(struct capture_node_s) );
    TRC( TRC_ID_MAIN_INFO, "max nodes of capture queue = %u", m_MaxNodes );

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;
}

capture_node_h CCaptureQueue::GetFreeNode(void)
{
    capture_node_h    pCaptureNode = 0;

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    for (uint32_t i = 0; i < m_MaxNodes; i++)
    {
        if (!m_pCaptureNodes[i].isUsed)
        {
            m_pCaptureNodes[i].isUsed = true;
            m_pCaptureNodes[i].pNextNode = 0;
            m_pCaptureNodes[i].pDataNode = 0;
            pCaptureNode = &m_pCaptureNodes[i];
            break;
        }
    }

    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    return pCaptureNode;
}

// Cut the queue just BEFORE the indicated node
bool CCaptureQueue::CutTail(capture_node_h node)
{
    capture_node_h   pCutNode = (capture_node_h)node;  // The queue will be cut just BEFORE this node
    capture_node_h   pNode;
    bool             nodeFound = false;

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    if ( (!pCutNode) || (!pCutNode->isUsed) )
    {
        TRC( TRC_ID_ERROR, "Tried to use an invalid node!" );
        goto exit_function;
    }

    // Look for this node in the Queue
    // NB: The node can be present only one time in the Queue otherwise the linked list would loop forever
    if (pCutNode == m_pNodeHead)
    {
        m_pNodeHead = 0;
        nodeFound    = true;
    }
    else
    {
        pNode = m_pNodeHead;

        while ( (pNode != 0) && (nodeFound == false) )
        {
            if (pNode->pNextNode == pCutNode)
            {
                pNode->pNextNode = 0;
                nodeFound = true;
            }
            pNode = pNode->pNextNode;
        }
    }

    if (!nodeFound)
    {
        TRC( TRC_ID_ERROR, "Tried to cut before a node not present in the Queue!" );
    }

exit_function:
    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    return (nodeFound);
}

// Function returning the node following the one provided in argument
// In case of null argument, it will provide the head of the queue
capture_node_h CCaptureQueue::GetNextNode(capture_node_h node)
{
    capture_node_h   pCurrentNode = node;
    capture_node_h   pNextNode;

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    // WARNING: The test condition is not the same as in other functions of this class because, here,
    // pCurrentNode can be null when we want to search from the beginning of the queue.
    // So, only the case where (pCurrentNode!=0) AND (pCurrentNode->isUsed==false) is an error.
    if ( (pCurrentNode) && (!pCurrentNode->isUsed) )
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
        if(pNextNode && !pNextNode->isUsed)
            pNextNode = 0;
    }
    else
    {
        pNextNode = pCurrentNode->pNextNode;
    }

    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    return (pNextNode);
}

bool CCaptureQueue::QueueNode(capture_node_h node)
{
    bool              res   = true;
    capture_node_h    pNode = node;
    capture_node_h    pTail;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    if ( (!pNode) || (!pNode->isUsed) )
    {
        TRC( TRC_ID_ERROR, "Tried to queue data to an invalid node!" );
        res = false;
        goto exit_queue_node;
    }

    // Queue it a the tail of the queue
    if (m_pNodeHead)
    {
        pTail = GetTail(m_pNodeHead);  // This can not fail
        pTail->pNextNode = pNode;
    }
    else
    {
        // This is the first node in the queue
        m_pNodeHead = pNode;
    }

    TRC( TRC_ID_UNCLASSIFIED, "Queued new buffer in node %p", pNode );

exit_queue_node:
    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );

    return res;
}

// Release a node
// The queue is updated to remove the reference to this node.
bool CCaptureQueue::ReleaseNode(capture_node_h node)
{
    bool               res   = true;
    capture_node_h     pNodeToRelease = node;
    capture_node_h     pNode;
    bool               nodeFound = false;

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    if ( (!pNodeToRelease) || (!pNodeToRelease->isUsed) )
    {
        TRC( TRC_ID_ERROR, "Tried to use an invalid node!" );
        res = false;
        goto exit_release_node;
    }

    // Remove any reference to this node in the Queue
    // NB: The node can be present only one time in the Queue otherwise the linked list would loop forever
    if (pNodeToRelease == m_pNodeHead)
    {
        m_pNodeHead = pNodeToRelease->pNextNode;
        nodeFound = true;
    }
    else
    {
        pNode = m_pNodeHead;

        while ( (pNode != 0) && (nodeFound == false) )
        {
            if (pNode->pNextNode == pNodeToRelease)
            {
                pNode->pNextNode = pNodeToRelease->pNextNode;
                nodeFound = true;
            }
            pNode = pNode->pNextNode;
        }
    }

    DoReleaseNode(pNodeToRelease);

exit_release_node:
    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    return (res);
}

bool CCaptureQueue::SetNodeData(capture_node_h node, void *pData)
{
    capture_node_h    pNode = node;
    bool              res   = true;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    if ( (!pNode) || (!pNode->isUsed) )
    {
        TRC( TRC_ID_ERROR, "Tried to set data to an invalid node!" );
        res = false;
        goto exit_queue_node;
    }

    pNode->pDataNode = pData;

exit_queue_node:
    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );

    return res;

}

void *CCaptureQueue::GetNodeData(capture_node_h node)
{
    capture_node_h    pNode = node;
    void             *pData = 0;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    if ( (!pNode) || (!pNode->isUsed) )
    {
        TRC( TRC_ID_ERROR, "Tried to get data from an invalid node!" );
        goto exit_queue_node;
    }

    pData = pNode->pDataNode;

exit_queue_node:
    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );

    return pData;
}


// Check if a node is valid
bool CCaptureQueue::IsValidNode(capture_node_h node)
{
    bool             res   = true;
    capture_node_h   pNode = node;

    if (m_QueueProtectedByLock)
        vibe_os_lock_resource(m_lock);

    // Check node validity
    if ( (!pNode) || (!pNode->isUsed) )
    {
        res = false;
    }

    if (m_QueueProtectedByLock)
        vibe_os_unlock_resource(m_lock);

    return res;
}


// Internal function which actually releases a node.
// It assumes that all the verification have previously been done by CCaptureQueue::ReleaseNode()
// so this function can never fail.
void CCaptureQueue::DoReleaseNode(capture_node_h node)
{
    capture_node_h    pNode = node;

    pNode->isUsed = false;

    if(pNode->pDataNode)
        vibe_os_free_memory(pNode->pDataNode);
    pNode->pDataNode = 0;
    pNode->pNextNode = 0;
}


capture_node_h CCaptureQueue::GetTail(capture_node_h node)
{
    capture_node_h    pNode = node;

    while (pNode->pNextNode)
    {
        pNode = pNode->pNextNode;
    }

    return (pNode);
}
