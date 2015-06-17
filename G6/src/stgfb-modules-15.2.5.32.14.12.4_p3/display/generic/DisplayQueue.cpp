/***********************************************************************
 *
 * File: display/generic/DisplayQueue.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>

#include "DisplayQueue.h"


CDisplayQueue::CDisplayQueue(void)
{
    m_pNodeHead             = 0;
}

CDisplayQueue::~CDisplayQueue()
{
#ifdef DEBUG
    // Check if the queue is empty
    if(!m_pNodeHead)
    {
        TRC( TRC_ID_ERROR, "Queue is not empty!!!" );
    }
#endif
}

bool CDisplayQueue::QueueDisplayNode(CNode *pNode)
{
    bool     res   = true;
    CNode   *pTail;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    // Check node validity
    if (!pNode)
    {
        TRC( TRC_ID_ERROR, "Tried to queue an invalid node!!!" );
        res = false;
        goto exit_queue_node;
    }

    // Queue it at the tail of the queue
    if (m_pNodeHead)
    {
        pTail = GetTail(m_pNodeHead);  // This cannot fail
        pTail->SetNextNode(pNode);
    }
    else
    {
        // This is the first node in the queue
        m_pNodeHead = pNode;
    }

exit_queue_node:

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );

    return res;
}

// Release a node
// The queue is updated to remove the reference to this node.
bool CDisplayQueue::ReleaseDisplayNode(CNode *pNodeToRelease)
{
    bool      res   = true;
    CNode    *pNode;
    bool      nodeFound = false;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    // Check node validity
    if (!pNodeToRelease)
    {
        TRC( TRC_ID_ERROR, "Tried to release an invalid node!!!" );

        res = false;
        goto exit_release_node;
    }

    // First, check if the node to release is part of the current Queue
    // In that case, update the queue to remove references to it
    if (pNodeToRelease == m_pNodeHead)
    {
        m_pNodeHead = pNodeToRelease->GetNextNode();
        pNodeToRelease->SetNextNode(0);
        nodeFound = true;
    }
    else
    {
        pNode = m_pNodeHead;
        while ( (pNode != 0) && (nodeFound == false) )
        {
            if (pNode->GetNextNode() == pNodeToRelease)
            {
                pNode->SetNextNode(pNodeToRelease->GetNextNode());
                nodeFound = true;
            }
            pNode = pNode->GetNextNode();
        }
    }

    // delete node if found in the queue
    if(nodeFound)
    {
        delete pNodeToRelease;
        pNodeToRelease = 0;
    }
    else
    {
        // The node was not found in the queue but this can happen if the queue was cut before this node (thanks to CutBeforeNode)
        // Delete the node if it is valid
        if(pNodeToRelease->IsNodeValid())
        {
            delete pNodeToRelease;
            pNodeToRelease = 0;
        }
    }

exit_release_node:
    TRCOUT( TRC_ID_UNCLASSIFIED, "" );
    return (res);
}

// Function returning the 1st node of the queue
CNode* CDisplayQueue::GetFirstNode(void)
{
    return(m_pNodeHead);
}

CNode *CDisplayQueue::GetLastNode(void)
{
    CNode   *pNode = m_pNodeHead;

    if(m_pNodeHead)
    {
        while (pNode->GetNextNode())
        {
                pNode = pNode->GetNextNode();
        }
        return (pNode);
    }
    else
        return m_pNodeHead;
}

// Similar function as GetLastNode() excepted that it starts from a given node
CNode *CDisplayQueue::GetTail(CNode *pNode)
{
    // Check node validity
    if (!pNode)
    {
        TRC( TRC_ID_ERROR, "Invalid node!" );
        return 0;
    }

    while (pNode->GetNextNode())
    {
            pNode = pNode->GetNextNode();
    }
    return (pNode);
}

// Cut the queue AFTER the indicated node
bool CDisplayQueue::CutAfterNode(CNode *pNode)
{
    // Check node validity
    if (!pNode)
    {
        TRC( TRC_ID_ERROR, "Invalid node!" );
        return false;
    }

    pNode->SetNextNode(0);

    return true;
}

// Cut the queue BEFORE the indicated node
bool CDisplayQueue::CutBeforeNode(CNode *pNode)
{
    CNode           *pCurrentNode;
    bool             nodeFound = false;

    // Check node validity
    if (!pNode)
    {
        TRC( TRC_ID_ERROR, "Invalid node!" );
        return false;
    }

    // Look for this node in the Queue
    // NB: The node can be present only one time in the Queue otherwise the linked list would loop forever
    if (pNode == m_pNodeHead)
    {
        m_pNodeHead = 0;
        nodeFound   = true;
    }
    else
    {
        pCurrentNode = m_pNodeHead;

        while ( (pCurrentNode != 0) && (nodeFound == false) )
        {
            if (pCurrentNode->GetNextNode() == pNode)
            {
                pCurrentNode->SetNextNode(0);
                nodeFound = true;
            }
            pCurrentNode = pCurrentNode->GetNextNode();
        }
    }

    if (!nodeFound)
    {
        TRC( TRC_ID_ERROR, "Tried to cut before a node not present in the Queue!" );
    }

    return (nodeFound);
}


