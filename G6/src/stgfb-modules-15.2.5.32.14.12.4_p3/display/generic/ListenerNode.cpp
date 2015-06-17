/***********************************************************************
 *
 * File: display/generic/ListenerNode.cpp
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <vibe_debug.h>
#include <vibe_os.h>

#include "stm_display.h"
#include "Node.h"
#include "ListenerNode.h"


CListenerNode::CListenerNode(const stm_ctrl_listener_t* params):CNode()
{
  m_params = *params;
}

CListenerNode::~CListenerNode(void)
{

}

