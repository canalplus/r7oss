/***********************************************************************
 *
 * File: display/generic/DisplayNode.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef DISPLAY_NODE_H
#define DISPLAY_NODE_H


#include "stm_display.h"
#include "Node.h"

class CNode;

enum BufferNodeType
{
    GNODE_BOTTOM_FIELD,
    GNODE_TOP_FIELD,
    GNODE_PROGRESSIVE
};


class CDisplayNode: public CNode
{
public:
    CDisplayNode(void);
    virtual ~CDisplayNode(void);

    bool                        m_isPictureValid;
    uint32_t                    m_pictureId;          // Unique Id given to each picture received
    stm_display_buffer_t        m_bufferDesc;         // Copy of the BufferDescriptor received from StreamingEngine or the Application
    BufferNodeType              m_srcPictureType;     // Is this node a Top Field, a Bottom Field or a Frame?
    char                        m_srcPictureTypeChar; // 'T', 'B' or 'F' depending if the picture is a Top field, Bottom field or Frame
    void *                      m_user;               // User who has queued this node
    bool                        m_isSkippedForFRC;    // Picture skipped by FRC. It can be used as reference by the Deinterlacer but it should not be displayed.

    // TO_DO: The following fields are set but currently not used!
    bool                        m_firstFieldOnly;
    bool                        m_repeatFirstField;
    BufferNodeType              m_firstFieldType;
};


#endif /* DISPLAY_NODE_H */

