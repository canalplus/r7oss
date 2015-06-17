/***********************************************************************
 *
 * File: display/generic/Node.cpp
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include "Node.h"

CNode::CNode(void)
{
    m_pNextNode = 0;
    m_magic_number = NODE_MAGIC_NUMBER;
}

CNode::~CNode(void)
{
    m_magic_number = 0;
}

