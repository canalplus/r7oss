/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef PIXEL_CAPTURE_QUEUE_H
#define PIXEL_CAPTURE_QUEUE_H


typedef struct capture_node_s *capture_node_h;

class CCaptureQueue
{
public:
               CCaptureQueue(uint32_t maxNodes, bool QueueProtectedByLock = false);
    virtual   ~CCaptureQueue(void);

    bool                Create(void);
    capture_node_h      GetFreeNode(void);

    capture_node_h      GetNextNode (capture_node_h node);
    bool                QueueNode   (capture_node_h node);
    bool                ReleaseNode (capture_node_h node);
    bool                CutTail     (capture_node_h node);

    bool                SetNodeData(capture_node_h node, void *pData);
    void               *GetNodeData(capture_node_h node);

    bool                IsValidNode(capture_node_h node);

protected:
    void              * m_lock;                         // Semaphore protecting the access to this queue
    bool                m_QueueProtectedByLock;         // Indicate if a lock is necessary to protect the access to this queue
    capture_node_h      m_pNodeHead;                    // Head of node queue

    uint32_t            m_MaxNodes;                     // Maximum node in the pool of nodes
    capture_node_h      m_pCaptureNodes;                // Pool of nodes

private:
    capture_node_h      GetTail(capture_node_h node);
    void                DoReleaseNode(capture_node_h node);
};


#endif /* PIXEL_CAPTURE_QUEUE_H */
