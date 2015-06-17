/***********************************************************************
 *
 * File: display/generic/ListenerNode.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef LISTENER_NODE_H
#define LISTENER_NODE_H




class CListenerNode: public CNode
{
public:
    CListenerNode(const stm_ctrl_listener_t* params);
    virtual ~CListenerNode(void);

protected:


private:
    stm_ctrl_listener_t m_params;

friend class CDisplayPlane;
};


#endif /* LISTENER_NODE_H */


