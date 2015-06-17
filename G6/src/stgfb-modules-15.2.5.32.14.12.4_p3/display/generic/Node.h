/***********************************************************************
 *
 * File: display/generic/Node.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef NODE_H
#define NODE_H

#define NODE_MAGIC_NUMBER 0x12345678

class CNode
{
public:
    CNode(void);
    virtual ~CNode(void);

    CNode*  GetNextNode(void)          const { return m_pNextNode; }

protected:
    // This method is protected so only the friend class CDisplayQueue is allowed to define the next node
    void    SetNextNode(CNode* pNode)        { m_pNextNode = pNode; }
    bool    IsNodeValid()                    { return( m_magic_number == NODE_MAGIC_NUMBER ); }

private:
    CNode   *m_pNextNode;
    uint32_t m_magic_number;

//Only CDisplayQueue is allowed to call SetNextNode()
friend class CDisplayQueue;
};


#endif /* NODE_H */


