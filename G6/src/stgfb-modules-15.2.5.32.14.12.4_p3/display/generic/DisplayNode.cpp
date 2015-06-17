/***********************************************************************
 *
 * File: display/generic/DisplayNode.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
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
#include "DisplayNode.h"

CDisplayNode::CDisplayNode(void):CNode()
{
    m_isPictureValid = false;
    m_pictureId      = 0;
    vibe_os_zero_memory(&m_bufferDesc,  sizeof(m_bufferDesc));
    m_srcPictureType     = GNODE_PROGRESSIVE;
    m_srcPictureTypeChar = 'F';

    m_firstFieldOnly     = false;
    m_repeatFirstField   = false;
    m_firstFieldType     = GNODE_PROGRESSIVE;
    m_user               = NULL;
    m_isSkippedForFRC    = false;
}

CDisplayNode::~CDisplayNode(void)
{
}


