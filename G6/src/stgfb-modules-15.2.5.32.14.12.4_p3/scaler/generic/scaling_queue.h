/***********************************************************************
 *
 * File: scaler/generic/scaling_queue.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALING_QUEUE_H
#define SCALING_QUEUE_H

#ifdef __cplusplus

#include "scaling_node.h"

class CScalingQueue
{
public:
                      CScalingQueue(element_type_t elementType);
    virtual          ~CScalingQueue(void);

    // Function returning the node following the one provided in argument
    // In case of null argument, it will provide the head of the queue
    CScalingNode     *GetNextScalingNode (CScalingNode *node);

    // Enqueue given node at the end of the queue
    bool             QueueScalingNode   (CScalingNode *node);

    // Dequeue given node from the queue
    bool             DequeueScalingNode (CScalingNode *node);

protected:
    element_type_t    m_elementType;   // type of scaling element manipulated by the queue
    CScalingNode     *m_pNodeHead;     // Head of node queue

private:
    CScalingNode     *GetTail(CScalingNode *node);
};

#endif /* __cplusplus */

#endif /* SCALING_QUEUE_H */
