/***********************************************************************
 *
 * File: display/generic/DisplayQueue.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef DISPLAY_QUEUE_H
#define DISPLAY_QUEUE_H

#include "Node.h"

class CNode;


class CDisplayQueue
{
public:
               CDisplayQueue(void);
    virtual   ~CDisplayQueue(void);

    // queue given node at the end of the queue
    bool       QueueDisplayNode   (CNode *pNode);

    // release given node from the queue
    bool       ReleaseDisplayNode (CNode *pNode);

    // Function returning the 1st node of the queue
    CNode     *GetFirstNode (void);

    // get the last valid node
    CNode     *GetLastNode(void);

    // Cut the queue AFTER the indicated node
    bool       CutAfterNode(CNode *pNode);

    // Cut the queue BEFORE the indicated node
    bool       CutBeforeNode(CNode *pNode);

private:
    CNode     *m_pNodeHead;                    // Head of node queue

    // get the tail node
    CNode     *GetTail(CNode *pNode);
};



#endif /* DISPLAY_QUEUE_H */

