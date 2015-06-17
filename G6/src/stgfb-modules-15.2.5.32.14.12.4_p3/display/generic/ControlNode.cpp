/***********************************************************************
 *
 * File: display/generic/ControlNode.cpp
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
#include "ControlNode.h"


CControlNode::CControlNode(stm_display_plane_control_t control, ControlParams* params):CNode()
{
    m_control = control;
    m_params = *params;
}

CControlNode::~CControlNode(void)
{

}

